# ESP32-S3 — головна каретка та сайт

Відкрийте `ESP32S3_Controller_Carriage_1.ino` в Arduino IDE.
У Boards Manager встановіть `esp32 by Espressif Systems` версії 3.x. Для цього скетча потрібні бібліотеки `ArduinoJson` 7.x (Benoit Blanchon) і `WebSockets` (Markus Sattler).

Налаштування Arduino IDE:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Upload Speed: `115200`
- Для першого тесту: `USB CDC On Boot: Disabled`

Плата створює Wi‑Fi `Motor-Control` з паролем `motor8266`, сайт відкривається за `http://motor-control.local/` або `http://192.168.4.1/`.

Плата керує власними моторами, сервоприводом і RGB та надсилає команди другій ESP32-S3 через ESP-NOW. Підключення й перший запуск описані в [інструкції для двох кареток](../../../README_TWO_CARRIAGES.md).
