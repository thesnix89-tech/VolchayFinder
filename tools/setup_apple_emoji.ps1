# Downloads AppleColorEmoji-Windows.ttf into src/MacDockShell/fonts/emoji/
# Font is gitignored — for local development and deploy only.

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$destDir = Join-Path $repoRoot "src\MacDockShell\fonts\emoji"
$destFile = Join-Path $destDir "AppleColorEmoji-Windows.ttf"

$apiUrl = "https://api.github.com/repos/samuelngs/apple-emoji-ttf/releases/latest"

Write-Host "Fetching latest apple-emoji-ttf release metadata..."
$release = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "MacDockShell-setup" }
$asset = $release.assets | Where-Object { $_.name -eq "AppleColorEmoji-Windows.ttf" } | Select-Object -First 1
if (-not $asset) {
    Write-Error "AppleColorEmoji-Windows.ttf not found in latest release."
}

$downloadUrl = $asset.browser_download_url
$expectedSize = [int64]$asset.size

New-Item -ItemType Directory -Force -Path $destDir | Out-Null

if (Test-Path $destFile) {
    $existing = (Get-Item $destFile).Length
    if ($existing -eq $expectedSize) {
        Write-Host "Already present: $destFile ($existing bytes)"
        exit 0
    }
    Write-Host "Existing file size mismatch ($existing vs $expectedSize); re-downloading..."
}

Write-Host "Downloading $($asset.name) ($([math]::Round($expectedSize / 1MB, 1)) MB)..."
Write-Host "URL: $downloadUrl"

$tmpFile = "$destFile.download"
if (Test-Path $tmpFile) { Remove-Item -Force $tmpFile }

Invoke-WebRequest -Uri $downloadUrl -OutFile $tmpFile -Headers @{ "User-Agent" = "MacDockShell-setup" }

$actualSize = (Get-Item $tmpFile).Length
if ($actualSize -lt 1MB) {
    Remove-Item -Force $tmpFile
    Write-Error "Download looks too small ($actualSize bytes)."
}

Move-Item -Force $tmpFile $destFile
Write-Host "Saved: $destFile ($actualSize bytes)"
