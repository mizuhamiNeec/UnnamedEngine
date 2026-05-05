## Summary

- 

## Scope

- [ ] Engine core (`src/engine`, `src/core`, build settings)
- [ ] App/runtime wiring (`src/app`, premake targets)
- [ ] Game content/runtime (`projects/*`)

## Separation Safety Checklist

- [ ] UE 本体向け変更では `premake --games=none` で生成/ビルド確認済み
- [ ] Game-specific 変更は対象ゲーム repo / ブランチに限定
- [ ] `game_profile.json` / `RegisterGameModule(...)` / runtime link の整合を確認

## Validation

- [ ] `generateallprojects.ps1` or `premake5.exe ... vs2026`
- [ ] `msbuild Unnamed.slnx /m /p:Configuration=Debug /p:Platform=x64`
- [ ] `msbuild Unnamed.slnx /m /p:Configuration=Develop /p:Platform=x64`
- [ ] 必要時 `--validate-startup-only`
