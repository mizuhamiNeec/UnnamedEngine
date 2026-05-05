# エンジン/ゲーム分離ガイド

このガイドは、エンジン本体（`UE`）とゲーム実装を段階的に分離して運用するための最短手順です。

## 1. 現在の分離機能

- `premake5.lua` の `--games` オプションで、同梱ゲーム Runtime/App のビルド対象を選択できます。
  - `--games=all` : すべて
  - `--games=parkour` : Parkour のみ
  - `--games=teamgame` : TeamGame のみ
  - `--games=none` : ゲーム非同梱（エンジンのみ）
- `UNNAMED_PROJECTS_ROOT` を使うと、`projects` ディレクトリをリポジトリ外から指定できます。
  - `game_profile.json` の探索先を repo ルート依存から分離できます。

## 2. 推奨運用（最短）

1. エンジン本体 repo は `--games=none` で運用する。  
2. 個人制作/チーム制作ゲームは別 repo に配置する。  
3. 起動時は `UNNAMED_PROJECTS_ROOT` で対象ゲーム repo の `projects` を指定する。  
4. 必要なゲーム Runtime をリンクした App を、ゲーム repo 側でビルドする。  

## 3. コマンド例

エンジンのみのプロジェクト生成:

```powershell
.\premake5.exe --games=none vs2026
# または
.\tools\generate-engine-only.ps1
```

TeamGame のみ有効化して生成:

```powershell
.\premake5.exe --games=teamgame vs2026
# または
.\tools\generate-teamgame-only.ps1
```

外部 `projects` ルートを指定して起動検証:

```powershell
$env:UNNAMED_PROJECTS_ROOT = "S:/Repositories/TD4_01/projects"
.\bin\Debug-windows-x86_64\UnnamedEditorApp\UnnamedEditorApp.exe --game=TeamGame --validate-startup-only
```

## 4. 注意点

- `game_profile.json` は profile/path 解決の仕組みです。Runtime 実体の自動ロード機構ではありません。
- ゲーム固有 Runtime を使うには、対象 Runtime をリンクした App 構成が必要です。
- `RegisterGameModule(...)` 未登録のゲームは `DefaultGameModule` 扱いになり、ゲーム固有 World/コンポーネント登録は動作しません。

## 5. 現状ロードマップ

1. **完了**: Parkour 復旧と分離基盤導入（`--games` / `UNNAMED_PROJECTS_ROOT`）。  
2. **完了**: UE 側 CI を engine-only（`--games=none`）へ切替。  
3. **完了**: ガードレール追加（`CODEOWNERS` / PR テンプレート）。  
4. **次**: `TD4_01` 側へ TeamGame 専用 CI を導入（`--games=teamgame` + startup validation）。  
5. **次**: UE 側の branch protection で `main` 直 push 禁止、必須レビューを有効化。  
