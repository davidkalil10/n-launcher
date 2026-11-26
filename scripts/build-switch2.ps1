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
    $flags=@('-std=c++17','-O2','-fPIE','-Wall','-Wextra','-Werror','-mcpu=cortex-a78c+crypto','-mtp=soft','-ftls-model=local-exec','-D__SWITCH__','-D__SWITCH2__','-ffunction-sections','-fdata-sections',"-specs=$sdk/libounce/switch.specs",'-Iinclude','-isystem',"$sdk/libounce/include",'-Wl,--gc-sections','-Wl,-Map,build/switch2/nx2-link-switch2.map')
    & "$sdk/devkitA64/bin/aarch64-none-elf-g++.exe" @flags src/input.cpp src/switch_main.cpp "-L$sdk/libounce/lib" "-L$sdk/portlibs/switch2/lib" -lkaihen -lnx -o build/switch2/nx2-link-switch2.elf
    if($LASTEXITCODE -ne 0){throw 'Switch 2 compile failed'}
    & "$sdk/tools/bin/nacptool.exe" --create 'NX2 Link - Input Probe' 'David Kalil Braga' '0.0.1' build/switch2/app.nacp
    if($LASTEXITCODE -ne 0){throw 'NACP creation failed'}
    & "$sdk/tools/bin/elf2nro.exe" build/switch2/nx2-link-switch2.elf dist/nx2-link-switch2.nro --nacp=build/switch2/app.nacp "--icon=$sdk/libounce/default_icon.jpg"
    if($LASTEXITCODE -ne 0){throw 'NRO creation failed'}
    python tools/verify_switch2_package.py dist/nx2-link-switch2.nro build/switch2/nx2-link-switch2.map
    if($LASTEXITCODE -ne 0){throw 'Switch 2 verification failed'}
    "$((Get-FileHash dist/nx2-link-switch2.nro -Algorithm SHA256).Hash.ToLowerInvariant())  nx2-link-switch2.nro" | Set-Content dist/SHA256SUMS.txt -Encoding ascii
} finally {$env:PATH=$oldPath;$env:DEVKITPRO=$oldDevkit;Pop-Location}
