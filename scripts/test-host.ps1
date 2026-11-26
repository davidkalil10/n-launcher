param([string]$Compiler = 'g++')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$oldPath=$env:PATH
$compilerPath=(Get-Command $Compiler -ErrorAction Stop).Source
$env:PATH=(Split-Path -Parent $compilerPath)+';'+$env:PATH
Push-Location $root
try {
    New-Item -ItemType Directory build/host -Force | Out-Null
    & $Compiler -std=c++17 -O2 -municode -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude src/input.cpp src/cli.cpp -o build/host/nx2link-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Host build failed'}
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude tests/test_module_lease.cpp -o build/host/module-lease-test.exe
    if($LASTEXITCODE -ne 0){throw 'Lease test build failed'}
    & build/host/module-lease-test.exe
    if($LASTEXITCODE -ne 0){throw 'Lease ownership tests failed'}
    python tests/test_input.py build/host/nx2link-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Reader tests failed'}
} finally {$env:PATH=$oldPath;Pop-Location}
