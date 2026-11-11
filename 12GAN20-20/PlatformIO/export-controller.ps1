# Build the main ESP32-S3 carriage once and create a complete 16 MB N16R8
# flash image for a different Windows PC. Nothing is uploaded by this script.
$ErrorActionPreference = 'Stop'

$projectDir = $PSScriptRoot
$pioHome = Join-Path $env:USERPROFILE '.platformio'
$pioExe = Join-Path $pioHome 'penv\Scripts\platformio.exe'
$pythonExe = Join-Path $pioHome 'penv\Scripts\python.exe'
$esptool = Join-Path $pioHome 'packages\tool-esptoolpy\esptool.py'
$bootApp = Join-Path $pioHome 'packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'

foreach ($path in @($pioExe, $pythonExe, $esptool)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing PlatformIO tool: $path" }
}

& $pioExe run --project-dir $projectDir --environment dual_esp32s3_controller
if ($LASTEXITCODE -ne 0) { throw 'PlatformIO build failed.' }

$buildDir = Join-Path $projectDir '.pio\build\dual_esp32s3_controller'
$bootloader = Join-Path $buildDir 'bootloader.bin'
$partitions = Join-Path $buildDir 'partitions.bin'
$firmware = Join-Path $buildDir 'firmware.bin'
foreach ($path in @($bootloader, $partitions, $bootApp, $firmware)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing flash image: $path" }
}

$releaseDir = Join-Path $projectDir '.pio\release'
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
$merged = Join-Path $releaseDir 'dual_esp32s3_controller-full.bin'

& $pythonExe $esptool --chip esp32s3 merge-bin `
    --flash-mode qio --flash-freq 80m --flash-size 16MB `
    -o $merged `
    0x0 $bootloader `
    0x8000 $partitions `
    0xe000 $bootApp `
    0x10000 $firmware
if ($LASTEXITCODE -ne 0) { throw 'Merging flash images failed.' }

Write-Output "Ready to copy: $merged"
Get-FileHash -LiteralPath $merged -Algorithm SHA256 | Select-Object Hash, Path
