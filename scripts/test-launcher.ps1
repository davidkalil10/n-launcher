param([string]$Compiler='g++')
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
    & $Compiler -std=c++17 -O2 -municode -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude -Ithird_party/zstd src/input.cpp src/cache.cpp tests/cache_probe.cpp build/host/zstd.o -o build/host/cache-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Cache probe build failed'}
    python tests/test_cache.py build/host/cache-probe.exe
    if($LASTEXITCODE -ne 0){throw 'Cache tests failed'}
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Itests/mock -Iinclude src/input.cpp src/native_input.cpp tests/native_backend_probe.cpp -o build/host/native-backend.exe
    if($LASTEXITCODE -ne 0){throw 'Native backend probe build failed'}
    python tests/test_native_backend.py build/host/native-backend.exe
    if($LASTEXITCODE -ne 0){throw 'Native backend tests failed'}
} finally {$env:PATH=$oldPath;Pop-Location}
