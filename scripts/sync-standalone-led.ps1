param([switch]$Check)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$files = @('config.h','firmware_impl.h','motor_output.h','platform_hal.h','web_ui.h')
foreach ($sketch in @('09_NodeMCU_MotorServoLED','10_WemosPro_MotorServoLED','11_ESP32S3_MotorServoLED')) {
  foreach ($file in $files) {
    $source = Join-Path $repo "standalone-led/$file"
    $dest = Join-Path $repo "ArduinoIDE/$sketch/$file"
    if ($Check) {
      if (!(Test-Path -LiteralPath $dest) -or (Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $dest).Hash) {
        throw "Out of sync: $dest"
      }
    } else { Copy-Item -LiteralPath $source -Destination $dest -Force }
  }
}
Write-Output 'Standalone LED: 3 self-contained sketches synchronized/checked.'
