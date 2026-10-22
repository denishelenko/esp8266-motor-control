param(
  [string]$Cli = "$env:LOCALAPPDATA/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe",
  [string]$Config = "$env:USERPROFILE/.arduinoIDE/arduino-cli.yaml",
  [string]$LibraryRoot = ""
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
  & "$PSScriptRoot/sync-standalone-led.ps1" -Check
  $targets = @(
    @{ Name='09_NodeMCU_MotorServoLED'; Board='esp8266:esp8266:nodemcuv2' },
    @{ Name='10_WemosPro_MotorServoLED'; Board='esp8266:esp8266:d1_mini_pro' },
    @{ Name='11_ESP32S3_MotorServoLED'; Board='esp32:esp32:esp32s3:UploadSpeed=115200,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=default' }
  )
  foreach ($target in $targets) {
    $args = @('--config-file', $Config, 'compile', '-b', $target.Board,
      '--warnings', 'all', '--build-path', ".build/$($target.Name)")
    if ($LibraryRoot) {
      $args += @('--library', (Join-Path $LibraryRoot 'WebSockets'),
        '--library', (Join-Path $LibraryRoot 'ArduinoJson'))
    }
    $args += "ArduinoIDE/$($target.Name)"
    Write-Output "BUILD $($target.Name)"
    & $Cli @args
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $($target.Name)" }
    Write-Output "PASS $($target.Name)"
  }
} finally { Pop-Location }
