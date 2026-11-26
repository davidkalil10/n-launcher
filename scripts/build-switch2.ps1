param([Parameter(Mandatory=$true)][string]$SdkRoot,[string]$SupportRoot,[string]$Python='python')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    & $Python -c "import PIL"
    if($LASTEXITCODE -ne 0){throw 'Install build requirements: python -m pip install -r requirements-build.txt'}
    & ./scripts/build-probe.ps1 -SdkRoot $SdkRoot -SupportRoot $SupportRoot
    & $Python tools/build_runtime.py --sdk $SdkRoot --support $SupportRoot
    if($LASTEXITCODE -ne 0){throw 'Generic runtime build failed'}
    $compiler=Join-Path $SdkRoot 'devkitA64/bin/aarch64-none-elf-g++.exe'
    & $Python tools/build_gui.py --compiler $compiler --sdk $SdkRoot --support $SupportRoot
    if($LASTEXITCODE -ne 0){throw 'N Launcher GUI build failed'}
    "$((Get-FileHash dist/n-launcher.nro -Algorithm SHA256).Hash.ToLowerInvariant())  n-launcher.nro" | Set-Content dist/SHA256SUMS.txt -Encoding ascii
} finally {Pop-Location}
