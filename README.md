<!-- プロジェクト名を記載 -->

## プロジェクト名

nfc出席管理システム

<!-- プロジェクトについて -->

## プロジェクトについて

本アプリケーションは，NFC 学生証を用いた出席管理システムである．
PCSCを用いてNFC UIDを取得し，SQLiteデータベースに出席記録を保存・表示する．
GUI は SDL2 と Dear ImGui を使って構築している．

<!-- プロジェクトの概要を記載 -->

## 動作環境

- macOS
- C++17
- CMake
- 以下のライブラリ：
  - [SDL2](https://www.libsdl.org/)
  - [Dear ImGui](https://github.com/ocornut/imgui)
  - [OpenGL 3.2+](https://www.opengl.org/)
  - [SQLite3](https://www.sqlite.org/)
  - [PCSC-Lite](https://pcsclite.apdu.fr/)

<!-- コンテナの作成方法、パッケージのインストール方法など、開発環境構築に必要な情報を記載 -->


