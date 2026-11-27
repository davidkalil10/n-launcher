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
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude src/input.cpp src/update.cpp src/http.cpp src/profile.cpp src/layout.cpp tests/test_app_core.cpp -o build/host/app-core-test.exe
    if($LASTEXITCODE -ne 0){throw 'App core build failed'}
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Iinclude tests/test_frame_acquire.cpp -o build/host/frame-acquire-test.exe
    if($LASTEXITCODE -ne 0){throw 'Frame acquisition test build failed'}
    & build/host/frame-acquire-test.exe
    if($LASTEXITCODE -ne 0){throw 'Frame acquisition tests failed'}
    & $Compiler -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror -Itests/tls_mock -Iinclude tests/test_tls_socket.cpp -o build/host/tls-socket-test.exe
    if($LASTEXITCODE -ne 0){throw 'TLS socket test build failed'}
    & build/host/tls-socket-test.exe
    if($LASTEXITCODE -ne 0){throw 'TLS socket tests failed'}
    $taskTemp=Join-Path $env:TEMP ('n-launcher-app-core-'+[guid]::NewGuid().ToString('N'))
    & build/host/app-core-test.exe $taskTemp
    if($LASTEXITCODE -ne 0){throw 'App core tests failed'}

} finally {$env:PATH=$oldPath;Pop-Location}
