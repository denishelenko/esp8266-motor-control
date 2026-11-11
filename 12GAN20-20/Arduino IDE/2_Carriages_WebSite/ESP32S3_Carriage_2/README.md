# ESP32-S3 — друга каретка

Відкрийте `ESP32S3_Carriage_2.ino` в Arduino IDE.

Налаштування Arduino IDE такі самі, як для головної плати: `ESP32S3 Dev Module`, 16 MB Flash, OPI PSRAM, Upload Speed 115200.
Потрібен пакет плат `esp32 by Espressif Systems` версії 3.x. ESP-NOW і PWM входять до цього пакета.

Плата знаходить головну ESP32-S3 через ESP-NOW, виконує команди й повертає статус. Підключення моторів, серво та RGB такі самі, як на [головній каретці](../../../README_TWO_CARRIAGES.md#піни-на-обох-каретках).
