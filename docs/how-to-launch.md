# 起動方法

- Editor:
  - `UnnamedEditorApp.exe --game=<GameName>`

!!! note "Note"
    `<GameName>` は `game_profile.json` の `gameName` または `aliases` に一致する名前を指定してください。
    UE 本体は engine-only 既定のため、必要に応じて `UNNAMED_PROJECTS_ROOT` または `--repo-root` で外部ゲームrepoを指してください。

- Game:
    - premake で定義したゲームプロジェクトの exe を直接起動

補助オプション:

- `--repo-root=<path>`: manifest 探索の repo root を明示
- `--validate-startup-only`: 起動導線検証だけ行って終了
