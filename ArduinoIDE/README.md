# Arduino IDE — скетчі проєкту

Кожна папка є окремим Arduino-скетчем. Відкривайте файл `.ino` з такою самою назвою, не змішуйте файли різних папок в одному скетчі.

## Основні варіанти

`03_OneESP` — одна NodeMCU керує сайтом, L298N і сервоприводом.

`09_NodeMCU_MotorServoLED` — актуальна автономна NodeMCU-версія з сайтом, мотором, серво і червоним LED: LED вимкнений у спокої та стробить під час руху.

`10_WemosPro_MotorServoLED` — така сама автономна версія для Wemos D1 mini Pro.

`11_ESP32S3_MotorServoLED` — така сама автономна версія для ESP32-S3 N16R8.

`01_TwoESP_Controller` — головна плата із сайтом і передаванням команд ESP-NOW.

`02_TwoESP_Motor` — друга NodeMCU, яка приймає ESP-NOW і керує мотором через L298N.

`12_TwoCarriages_ESP32_Controller` + `13_TwoCarriages_NodeMCU` — окрема система двох кареток.

## Калібрування і стендові тести

- `04_Calibration` — вимірювання швидкості за реальною відстанню.
- `05_Motor_OneSecond_Test` — простий тест обертання мотора.
- `06_TwoMotors_OneESP` — два мотори через два мости одного L298N.
- `14_Travel_Time_Test` — час проходження робочих 16 м.
- `15_Servo_Travel_Test` — тест сервопривода.

## Прошивання

1. В Arduino IDE відкрийте потрібний `.ino`.
2. Оберіть правильну плату: NodeMCU 1.0, LOLIN(WEMOS) D1 mini Pro або ESP32-S3 Dev Module.
3. Оберіть COM-порт.
4. Для першого завантаження використовуйте Upload Speed 115200.
5. Serial Monitor відкривайте на 115200 baud.

Потрібні бібліотеки: **ArduinoJson 7.x** і **WebSockets by Markus Sattler**. Wi-Fi, WebServer, ESP-NOW і Servo для ESP8266 входять до пакета плат.

## Типові піни NodeMCU

Для `09_NodeMCU_MotorServoLED`:

- D1 → ENA L298N
- D2 → IN1 L298N
- D5 → IN2 L298N
- D6 → сигнал сервопривода
- D7 → червоний LED через резистор
- GND → спільна земля

Зелений і синій канали RGB не підключаються.

Робоча площа задається в `config.h` як `TRACK_LENGTH_M = 16.0f`. Без енкодера координата є розрахунковою, тому власні вимірювання потрібно внести в `SPEED_CALIBRATION`.
