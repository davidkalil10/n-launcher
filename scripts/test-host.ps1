param([string]$Compiler = 'g++')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$oldPath=$env:PATH
$compilerPath=(Get-Command $Compiler -ErrorAction Stop).Source
$env:PATH=(Split-Path -Parent $compilerPath)+';'+$env:PATH
Push-Location $root
try {
    New-Item -ItemType Directory build/host -Force | Out-Null
    $cc=Join-Path (Split-Path -Parent $compilerPath) 'gcc.exe'
    & $cc -O2 -DZSTD_DISABLE_ASM -c third_party/zstd/zstddeclib.c -o build/host/zstd.o
    if($LASTEXITCODE -ne 0){throw 'Decoder build failed'}
    & $Compiler -std=c++17 -O2 -municode -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude -Ithird_party/zstd src/input.cpp src/program.cpp src/cli.cpp build/host/zstd.o -o build/host/nx2link-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Host build failed'}
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude tests/test_module_lease.cpp -o build/host/module-lease-test.exe
    if($LASTEXITCODE -ne 0){throw 'Lease test build failed'}
    & build/host/module-lease-test.exe
    if($LASTEXITCODE -ne 0){throw 'Lease ownership tests failed'}
    python tests/test_program.py build/host/nx2link-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Program preparation tests failed'}
    python tests/test_input.py build/host/nx2link-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Reader tests failed'}
} finally {$env:PATH=$oldPath;Pop-Location}
