# 新規ゲーム作成ガイド

この手順は「空ゲーム追加 -> 最小シーン作成 -> 独自コンポーネント追加 -> Editor 起動確認」までの最短導線です。

## 1. 雛形生成

`tools/newgame.ps1` を使います。

外部ゲームrepoの `projects` へ直接生成する（推奨）:

```powershell
.\tools\newgame.ps1 -Name MyGame -Alias My -ProjectsRoot "S:/Repositories/TD4_01/projects"
```

UE リポジトリ内に生成する場合（検証用のみ）:

```powershell
.\tools\newgame.ps1 -Name MyGame -Alias My
```

生成される主な内容:
- `projects/MyGame/runtime` (最小 GameModule + ComponentRegistration + SampleComponent)
- `projects/MyGame/content/scenes/bootstrap.json`
- `projects/MyGame/config/game_profile.json`

## 2. エンジン配線

生成後、以下は手動追加が必要です。

1. `premake5.lua`
   - `MyGameRuntime` を追加
   - `UnnamedEditorApp` に include/link 追加
2. `src/app/EditorMain.cpp`
   - `RegisterGameModule("MyGame", &CreateMyGameGameModule)` を追加
   - 必要なら alias 追加
3. （必要なら）専用 standalone app を追加
   - `TeamGameMain.cpp` 相当のエントリポイントを用意

## 2.1 運用ルール（重要）

- ゲーム固有アセットは `projects/<Game>/content` に配置します。
- `game_profile.json` はプロファイル探索（`gameRoot` / `contentRoot` / `configRoot` / `aliases`）を担いますが、Runtime 実体の自動ロード機構ではありません。
- 実行時にゲーム固有 Runtime（World / コンポーネント登録）を使うには、各 App 側で `RegisterGameModule(...)` による生成関数登録が必要です。
- ゲーム固有コンポーネントは `IGameModule::RegisterGameComponents(...)` の明示登録を正規経路とします。`REGISTER_COMPONENT(...)` のみへ依存すると、App 側リンク構成次第で静的初期化が到達しない場合があります。

根拠コード:
- `src/app/GameModuleFactory.cpp`
  - `CreateGameModule(...)` で `createFunction == nullptr` の場合、`DefaultGameModule` を生成
- `src/app/EditorMain.cpp` / `src/app/GameMain.cpp` / `src/app/TeamGameMain.cpp`
  - `RegisterGameModule(...)` で Runtime 生成関数を登録
- `premake5.lua`
  - `TeamGameApp` は `/WHOLEARCHIVE:TeamGameRuntime.lib` 指定がないため、明示登録経路を維持する運用が安全

## 3. 再生成とビルド

```powershell
.\generateallprojects.ps1
msbuild Unnamed.slnx /m /p:Configuration=Debug /p:Platform=x64
```

## 4. 起動確認

```powershell
.\bin\Debug-windows-x86_64\UnnamedEditorApp\UnnamedEditorApp.exe --game=MyGame
```

または導線検証のみ:

```powershell
.\bin\Debug-windows-x86_64\UnnamedEditorApp\UnnamedEditorApp.exe --game=MyGame --validate-startup-only
```

## 5. よくある失敗

- `Unknown component type` が出る
  - `GetStableName()` と scene JSON の `components[].type` が不一致
  - `RegisterGameComponents` で登録漏れ
  - runtime link/premake 追加漏れ

- game が見つからない
  - `projects/<Game>/config/game_profile.json` 不備
  - `gameName` / `aliases` と `--game` の不一致
  - `--repo-root` が誤っている

