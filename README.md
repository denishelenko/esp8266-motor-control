# 12GAN20-20 — система керування каретками

Проєкт для Arduino IDE та VS Code + PlatformIO знаходиться у [12GAN20-20](12GAN20-20).

```text
12GAN20-20/
  Arduino IDE/
    1_Carriage_WebSite/
      NodeMCU/
      Wemos/
      ESP32S3/
    2_Carriages_WebSite/
      ESP32S3_Controller_Carriage_1/
      ESP32S3_Carriage_2/
  PlatformIO/
    platformio.ini
  web-interface-demo.html
  web-interface-two-carriages-demo.html
```

## Який скетч прошивати

В Arduino IDE відкрийте відповідний `.ino` з [Arduino IDE](<12GAN20-20/Arduino IDE>). У VS Code відкрийте [PlatformIO](12GAN20-20/PlatformIO) як папку проєкту. Обидва середовища використовують спільні вихідні файли в `Arduino IDE`.

| Збірка | Скетч Arduino IDE | Профіль PlatformIO |
| --- | --- | --- |
| Одна каретка, NodeMCU v3 | `1_Carriage_WebSite/NodeMCU/NodeMCU.ino` | `single_nodemcu` |
| Одна каретка, Wemos D1 mini Pro | `1_Carriage_WebSite/Wemos/Wemos.ino` | `single_d1_mini_pro` |
| Одна каретка, ESP32-S3 | `1_Carriage_WebSite/ESP32S3/ESP32S3.ino` | `single_esp32s3` |
| Дві каретки, головна ESP32-S3 із сайтом | `2_Carriages_WebSite/ESP32S3_Controller_Carriage_1/ESP32S3_Controller_Carriage_1.ino` | `dual_esp32s3_controller` |
| Дві каретки, віддалена ESP32-S3 | `2_Carriages_WebSite/ESP32S3_Carriage_2/ESP32S3_Carriage_2.ino` | `dual_esp32s3_remote` |

Локальні HTML-файли — це лише візуальні демо для браузера, вони не керують реальною платою.

Wi-Fi для актуальних скетчів: `Motor-Control`, пароль `motor8266`, адреса `http://192.168.4.1`.

У парі двох ESP32-S3 телефон підключається до Wi-Fi головної плати. Плати обмінюються командами й статусом через ESP-NOW. Підключення моторів та інструкція тестування наведені в [README_TWO_CARRIAGES.md](12GAN20-20/README_TWO_CARRIAGES.md).

Робоча площа троса: 16 м, орієнтовний діапазон від центру `−8…+8 м`. Власні вимірювання швидкості потрібно внести у `config.h` відповідного скетчу.

В автономному скетчі тести на 1–4 секунди знаходяться в розділі «Ручне калібрування». Виміряйте пройдену відстань рулеткою для кожного напрямку.

Для підключення однокареткової прошивки до домашнього Wi‑Fi використовуйте профіль [Home dev internet](12GAN20-20/Home%20dev%20internet). За наявності локального `home_wifi.local.h` плата працює в режимі `AP + STA`: сайт доступний і через `Motor-Control`, і через домашню мережу.

Додаткові інструкції для [PlatformIO](12GAN20-20/PlatformIO/README.md) та [однієї каретки](12GAN20-20/README_STANDALONE_LED.md).
