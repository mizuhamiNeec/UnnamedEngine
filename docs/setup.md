# 初回セットアップ

## 1. リポジトリをクローン

```bash
git clone https://github.com/mizuhamiNeec/UnnamedEngine.git
```

## 2. [premake5 をダウンロード](https://premake.github.io/download) し、 `premake5.exe` をリポジトリルートに配置

!!! note "Note"
    すでにpremake5.exeを環境変数に通してある場合はこの手順をスキップできます。

## 3. `generateallprojects.ps1` を実行してプロジェクト生成
- プロジェクト生成
```powershell
./generateallprojects.ps1 # UE本体向けに --games=none でプロジェクト生成します
```
または
```powershell
./premake5.exe --games=none vs2026 # premake5.exeをプロジェクトに配置している場合

premake5.exe --games=none vs2026 # すでに環境変数に通してある場合
```

ゲームrepo側の `projects` を参照してゲーム Runtime/App まで生成する場合:
```powershell
premake5.exe --games=teamgame --projects-root="S:/Repositories/TD4_01/projects" vs2026
```

## 4. お好みのIDEでソリューションを開く
- `VisualStudio 2026` | `Rider` の場合は 生成された `.slnx` ファイルを開いてください。

!!! warning "注意"
    `VisualStudio Code` をお使いの方はアクションが整備されていないため、IDEでの実行を推奨します。どうしてもVSCodeで実行したい場合は、管理者へ。
