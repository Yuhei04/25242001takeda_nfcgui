#include <iterator>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <sqlite3.h>
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>


// AttendanceManager クラス
class AttendanceManager {
public:
    struct Entry {
        std::string uid;
        std::string timestamp;
        std::string student_id;
        std::string name;
    };

    bool load() {
        entries.clear();
        sqlite3* db;
        if (sqlite3_open("attendance.db", &db) != SQLITE_OK) return false;

        const char* sql = R"(
            SELECT attendance.uid, attendance.timestamp, students.student_id, students.name
            FROM attendance
            LEFT JOIN students ON attendance.uid = students.uid
            ORDER BY timestamp DESC
            LIMIT 50;
        )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* uid  = sqlite3_column_text(stmt, 0);
                const unsigned char* ts   = sqlite3_column_text(stmt, 1);
                const unsigned char* sid  = sqlite3_column_text(stmt, 2);
                const unsigned char* name = sqlite3_column_text(stmt, 3);

                entries.push_back({
                    uid  ? reinterpret_cast<const char*>(uid)  : "",
                    ts   ? reinterpret_cast<const char*>(ts)   : "",
                    sid  ? reinterpret_cast<const char*>(sid)  : "",
                    name ? reinterpret_cast<const char*>(name) : ""
                });
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);
        return true;
    }

    const std::vector<Entry>& getEntries() const { return entries; }
    std::vector<Entry>::const_iterator begin() const { return entries.begin(); }
    std::vector<Entry>::const_iterator end() const { return entries.end(); }

private:
    std::vector<Entry> entries;
};

std::string readUID() {
    SCARDCONTEXT hContext;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    char mszReaders[1024];
    DWORD dwReaders = sizeof(mszReaders);

    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext) != SCARD_S_SUCCESS)
        return "";

    if (SCardListReaders(hContext, nullptr, mszReaders, &dwReaders) != SCARD_S_SUCCESS) {
        SCardReleaseContext(hContext);
        return "";
    }

    if (SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED,
                     SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                     &hCard, &dwActiveProtocol) != SCARD_S_SUCCESS) {
        SCardReleaseContext(hContext);
        return "";
    }

    BYTE cmdGetUID[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };
    BYTE recvBuffer[256];
    DWORD recvLength = sizeof(recvBuffer);
    const SCARD_IO_REQUEST* pioSendPci =
        (dwActiveProtocol == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

    LONG result = SCardTransmit(hCard, pioSendPci,
                                cmdGetUID, sizeof(cmdGetUID),
                                nullptr, recvBuffer, &recvLength);

    std::string uid = "";
    if (result == SCARD_S_SUCCESS && recvLength > 2) {
        for (DWORD i = 0; i < recvLength - 2; ++i) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", recvBuffer[i]);
            uid += buf;
        }
    }

    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hContext);
    return uid;
}


bool saveAttendance(const std::string& uid) {
    sqlite3* db;
    if (sqlite3_open("attendance.db", &db) != SQLITE_OK) {
        return false;
    }

    const char* sql = "INSERT INTO attendance (uid, timestamp) VALUES (?, datetime('now', 'localtime'));";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_TRANSIENT);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return success;
}

void initializeDB() {
    sqlite3* db;
    if (sqlite3_open("attendance.db", &db) == SQLITE_OK) {
        const char* sql = "CREATE TABLE IF NOT EXISTS attendance (uid TEXT, timestamp TEXT);";
        sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }
}

int main(int, char**) {
    initializeDB();
    
    // SDL2初期化
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // OpenGLバージョン指定
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // ウィンドウ作成
    SDL_Window* window = SDL_CreateWindow("NFC 出席管理", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // 垂直同期ON

    // ImGui初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontFromFileTTF("/Users/yuheitakeda/Desktop/プログラミング演習Ⅱ/cpp/NFC_GUI/fonts/NotoSansJP-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

    ImGui::StyleColorsDark(); // ダークテーマ
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // メインループ開始
    std::string currentUID = "";
    AttendanceManager manager;

    auto lastUpdate = std::chrono::steady_clock::now();

    bool showWindow = true;
    while (showWindow) {
        // イベント処理
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                showWindow = false;
        }

        // データ更新（2秒ごと）
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 2) {
            currentUID = readUID(); // 実際には NFC 読み取りに差し替え
            manager.load(); // 実際には SQLite 読み込みに差し替え
            lastUpdate = now;
        }

        // ImGuiフレーム開始
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ウィンドウUI
        ImGui::Begin("出席情報");

        ImGui::Text("現在のUID:");
        if (ImGui::Button("出席を記録")) {
            if (!currentUID.empty()) {
                if (saveAttendance(currentUID)) {
                    manager.load();  // 保存後に履歴を更新
                } else {
                    std::cerr << "保存に失敗しました\n";
                }
            }
        }
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", currentUID.c_str());

        ImGui::Separator();
        ImGui::Text("出席履歴:");
        if (ImGui::BeginTable("AttendanceTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("UID");
            ImGui::TableSetupColumn("学修番号");
            ImGui::TableSetupColumn("氏名");
            ImGui::TableSetupColumn("時刻");
            ImGui::TableHeadersRow();

            for (const auto& entry : manager) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", entry.uid.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", entry.student_id.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%s", entry.name.c_str());
                ImGui::TableSetColumnIndex(3); ImGui::Text("%s", entry.timestamp.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::End(); // 出席情報ウィンドウ

        // 描画
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // 終了処理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
