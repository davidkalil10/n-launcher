param([Parameter(Mandatory=$true)][string]$SdkRoot,[string]$SupportRoot)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$oldPath=$env:PATH;$oldDevkit=$env:DEVKITPRO
Push-Location $root
try {
    $sdk=(Resolve-Path -LiteralPath $SdkRoot).Path.Replace('\','/')
    $env:DEVKITPRO=$sdk
    $env:PATH="$SupportRoot;$sdk/devkitA64/bin;$sdk/tools/bin;$env:PATH"
    New-Item -ItemType Directory build/switch2,dist -Force | Out-Null
    $version = ((Get-Content include/nx2link/version.hpp -Raw) -split '"')[1]
    New-Item -ItemType Directory build/generated -Force | Out-Null
    & "$sdk/devkitA64/bin/aarch64-none-elf-gcc.exe" -c -mcpu=cortex-a78c+crypto fixtures/module.S -o build/switch2/fixture.o
    if($LASTEXITCODE -ne 0){throw 'Fixture assembly failed'}
    & "$sdk/devkitA64/bin/aarch64-none-elf-objcopy.exe" -O binary --only-section=.text build/switch2/fixture.o build/switch2/fixture.bin
    if($LASTEXITCODE -ne 0){throw 'Fixture image generation failed'}
    python tools/embed_fixture.py build/switch2/fixture.bin build/generated/fixture.hpp
    if($LASTEXITCODE -ne 0){throw 'Fixture layout verification failed'}
    & "$sdk/devkitA64/bin/aarch64-none-elf-gcc.exe" -c -O2 -fPIE -mcpu=cortex-a78c+crypto -mtp=soft -ffunction-sections -fdata-sections -DZSTD_DISABLE_ASM third_party/zstd/zstddeclib.c -o build/switch2/zstd.o
    if($LASTEXITCODE -ne 0){throw 'Zstd decoder compile failed'}
    $flags=@('-std=c++17','-O2','-fPIE','-Wall','-Wextra','-Werror','-mcpu=cortex-a78c+crypto','-mtp=soft','-ftls-model=local-exec','-D__SWITCH__','-D__SWITCH2__','-ffunction-sections','-fdata-sections',"-specs=$sdk/libounce/switch.specs",'-Iinclude','-Ibuild/generated','-Ithird_party/zstd','-isystem',"$sdk/libounce/include",'-Wl,--gc-sections','-Wl,-Map,build/switch2/nx2-link-switch2.map')
    & "$sdk/devkitA64/bin/aarch64-none-elf-g++.exe" @flags src/input.cpp src/program.cpp src/loader_probe.cpp src/switch_main.cpp build/switch2/zstd.o "-L$sdk/libounce/lib" "-L$sdk/portlibs/switch2/lib" -lkaihen -lnx -o build/switch2/nx2-link-switch2.elf
    if($LASTEXITCODE -ne 0){throw 'Switch 2 compile failed'}
    & "$sdk/tools/bin/nacptool.exe" --create 'N Launcher - Diagnostics' 'David Kalil Braga' $version build/switch2/app.nacp
    if($LASTEXITCODE -ne 0){throw 'NACP creation failed'}
    & "$sdk/tools/bin/elf2nro.exe" build/switch2/nx2-link-switch2.elf dist/loader-probe.nro --nacp=build/switch2/app.nacp "--icon=$sdk/libounce/default_icon.jpg"
    if($LASTEXITCODE -ne 0){throw 'NRO creation failed'}
    python tools/verify_switch2_package.py dist/loader-probe.nro build/switch2/nx2-link-switch2.map
    if($LASTEXITCODE -ne 0){throw 'Switch 2 verification failed'}
    "$((Get-FileHash dist/loader-probe.nro -Algorithm SHA256).Hash.ToLowerInvariant())  loader-probe.nro" | Set-Content dist/probe-SHA256SUMS.txt -Encoding ascii
} finally {$env:PATH=$oldPath;$env:DEVKITPRO=$oldDevkit;Pop-Location}
