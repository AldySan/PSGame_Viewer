# Build a portable Windows archive of PSGame-Viewer: windeployqt collects
# the Qt runtime next to the executable; the archive runs from any folder
# without an installer. Everything the app writes stays in that folder
# (PSGame-Viewer.ini, library.db, cache\, batch rename logs).
#
# Usage (from the repository root, in a Qt-enabled environment):
#   pwsh packaging/make-portable.ps1 [-BuildDir build]
param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Exe = Join-Path $BuildDir "psgame-viewer.exe"
if (-not (Test-Path $Exe)) {
    Write-Host "building first..."
    cmake --build $BuildDir
    if (-not (Test-Path $Exe)) { throw "psgame-viewer.exe not found in $BuildDir" }
}

$Version = (Select-String -Path (Join-Path $Root "CMakeLists.txt") `
    -Pattern "project\(PSGameViewer VERSION ([0-9.]+)").Matches[0].Groups[1].Value
$Name = "PSGame-Viewer-$Version-win64-portable"
$Stage = Join-Path (New-Item -ItemType Directory -Path ([IO.Path]::GetTempPath()) `
    -Name ([Guid]::NewGuid().ToString("N"))) $Name
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

# 1. the executable + Qt runtime
Copy-Item $Exe $Stage
$deploy = "windeployqt.exe"
if (-not (Get-Command windeployqt -ErrorAction SilentlyContinue)) {
    throw "windeployqt not found in PATH (run from a Qt environment)"
}
& $deploy --release --no-compiler-runtime --qmldir (Join-Path $Root "src/qml") `
    (Join-Path $Stage "psgame-viewer.exe") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# 1b. MSVC builds: copy the VC runtime DLLs so the archive also runs on
#     machines without the redistributable installed (no-op for MinGW or
#     when the script runs outside a Visual Studio environment)
if ($env:VCToolsRedistDir -and (Test-Path $env:VCToolsRedistDir)) {
    $crt = Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir "x64") `
        -Recurse -Include "msvcp140.dll", "vcruntime140.dll", `
                     "vcruntime140_1.dll" -ErrorAction SilentlyContinue
    if ($crt) {
        $crt | Copy-Item -Destination $Stage
        Write-Host "VC runtime DLLs:"
        $crt | ForEach-Object { Write-Host "  $($_.Name)" }
    }
}

# 2. portable hint file for the archive users
@(
    "PSGame-Viewer (portable)",
    "",
    "Run psgame-viewer.exe. All settings and data stay inside this folder:",
    "  PSGame-Viewer.ini   settings",
    "  library.db          game library",
    "  cache\              cover art cache",
    "  batch_rename_*.log  rename revert logs"
) | Set-Content (Join-Path $Stage "README.txt")

Compress-Archive -Path $Stage -DestinationPath (Join-Path $Root "$Name.zip")
Write-Host "wrote $Name.zip"
