# ESP32-S3 — головна каретка та сайт

Відкрийте `ESP32S3_Controller_Carriage_1.ino` в Arduino IDE.

Налаштування Arduino IDE:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Upload Speed: `115200`
- Для першого тесту: `USB CDC On Boot: Disabled`

Плата створює Wi‑Fi `Motor-Control` з паролем `motor8266`, сайт відкривається за `http://motor-control.local/` або `http://192.168.4.1/`.

Вона керує власними моторами через L9110S і надсилає команди другій ESP32-S3 через UDP.
