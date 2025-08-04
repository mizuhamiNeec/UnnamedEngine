Write-Host "プロジェクトを生成しています..."

try {
    premake5.exe vs2022
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Premake5 の実行が完了しました。"
    } else {
        Write-Host "Premake5 の実行中にエラーが発生しました。"
        exit 1
    }
} catch {
    Write-Host "premake5.exe の実行に失敗しました。最新版のpremakeをダウンロードしてシステム環境変数にPATHを追加してください。"
    exit 1
}
