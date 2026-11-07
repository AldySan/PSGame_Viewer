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
& $deploy --release --no-compiler-runtime --no-opengl-sw --no-translations `
    --qmldir (Join-Path $Root "src/qml") `
    (Join-Path $Stage "psgame-viewer.exe") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# 1a. trim what the app never loads. Every QML file pins
#     QtQuick.Controls.Basic, so the other styles and their root DLLs
#     are dead weight; qmltooling is the QML debugger; only SQLite is
#     used; opengl32sw/dxcompiler/dxil serve fallback rendering paths
#     the app does not take (D3D11 is the default RHI backend on
#     Windows 10+) — the same cuts the Linux archive makes.
$unusedStyles = @("FluentWinUI3", "Fusion", "Imagine", "Material",
                   "Universal", "Windows", "macOS", "iOS")
foreach ($s in $unusedStyles) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue `
        (Join-Path $Stage "qml/QtQuick/Controls/$s")
}
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue `
    (Join-Path $Stage "qml/QtQuick/NativeStyle")
Get-ChildItem (Join-Path $Stage "qml/QtQuick/Controls") -Directory `
    -Filter designer -ErrorAction SilentlyContinue |
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
# root style DLLs: keep Controls2, Basic, BasicStyleImpl and Impl
$keep = @("Qt6QuickControls2.dll", "Qt6QuickControls2Basic.dll",
          "Qt6QuickControls2BasicStyleImpl.dll", "Qt6QuickControls2Impl.dll")
Get-ChildItem $Stage -Filter "Qt6QuickControls2*.dll" | Where-Object {
    $keep -notcontains $_.Name } | Remove-Item -Force
# the D3D12 shader compiler pair is loaded dynamically by Qt6Gui and
# only used by the D3D12 RHI backend, which is never selected by default
Remove-Item -Force -ErrorAction SilentlyContinue `
    (Join-Path $Stage "dxcompiler.dll"), (Join-Path $Stage "dxil.dll")
# QML debugger plugins never ship
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue `
    (Join-Path $Stage "qmltooling")
# only the SQLite driver is used (library.db)
Get-ChildItem (Join-Path $Stage "sqldrivers") -Filter *.dll `
    -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "qsqlite.dll" } |
    Remove-Item -Force -ErrorAction SilentlyContinue

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
