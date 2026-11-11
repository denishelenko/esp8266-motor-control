# Прошивання через VS Code + PlatformIO

1. Встановіть розширення **PlatformIO IDE** у VS Code.
2. Відкрийте як проєкт папку, у якій лежить цей `platformio.ini`.
3. У меню **PlatformIO → Project Tasks** виберіть профіль і натисніть **Build**, потім **Upload**. Перший запуск завантажує пакети плат і бібліотеки.

| Профіль | Плата й роль |
| --- | --- |
| `single_nodemcu` | NodeMCU v3, одна каретка та сайт |
| `single_d1_mini_pro` | Wemos D1 mini Pro, одна каретка та сайт |
| `single_esp32s3` | ESP32-S3 N16R8, одна каретка та сайт |
| `dual_esp32s3_controller` | ESP32-S3 N16R8, головна каретка та сайт |
| `dual_esp32s3_remote` | ESP32-S3 N16R8, віддалена каретка |
| `dual_home_main_serial` / `dual_home_main_ota` | Головна ESP32-S3 у зовнішній мережі `Motor-Control`, USB / OTA |
| `dual_home_remote_serial` / `dual_home_remote_ota` | Друга ESP32-S3 у тій самій мережі, USB / OTA |

Усі активні ESP32-S3 прошивки використовують однакові піни: `1` → IN1 обох мостів, `2` → IN2 обох мостів, `42` → A1 TXS0108E для сервопривода, `47` → червоний LED, `21` → зелений LED. Профілі PlatformIO підключають відповідні скетчі з папки `Arduino IDE` або `Home dev internet`, тому окремих копій пінів у `src` немає.

Для двох кареток прошийте головну й віддалену плати відповідними профілями. Для ESP32-S3 задані 16 МБ flash, OPI PSRAM і прошивання через USB-UART. Номер COM визначається автоматично. За потреби додайте `upload_port = COM6` у потрібний профіль, підставивши порт плати. Monitor працює на 115200 бод.

### Якщо Upload зупиняється на `Connecting... No serial data received`

Це збій зв'язку з завантажувачем до запису прошивки. Для плати **N16R8** залишайте 16 МБ flash і 8 МБ PSRAM: число 6 553 600 байтів у звіті збірки — розмір розділу для застосунку, а не розмір флеш-пам'яті. У профілі `dual_esp32s3_controller` на час першого підключення встановлено `upload_speed = 115200`; після успішного прошивання можна спробувати 460800 або 921600.

1. Вимкніть батарею та зовнішнє живлення приводів. Підключіть ESP32-S3 справним кабелем без USB-хаба до роз'єму **UART** плати.
2. У Windows перевірте, який COM-порт з'являється саме при підключенні цієї плати. Якщо автоматичний вибір PlatformIO помилковий, впишіть цей порт у `upload_port` профілю `dual_esp32s3_controller`.
3. Закрийте Serial Monitor та інші програми, що використовують порт. Запустіть **Upload**. Якщо на екрані `Connecting...` немає відповіді, утримуйте **BOOT**, коротко натисніть **RESET**, відпустіть RESET, потім BOOT.
4. Якщо плата досі не відповідає, спробуйте інший USB-кабель або роз'єм **USB** ESP32-S3 і знову перевірте номер COM-порту. Не міняйте розмір флешу на 32 МБ, якщо на модулі позначено N16R8.

Інструкції виробника: [ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html), [esptool: No serial data received](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/troubleshooting.html).

З термінала PlatformIO:

```powershell
pio run -e dual_esp32s3_controller
pio run -e dual_esp32s3_controller -t upload
pio run -e dual_esp32s3_remote -t upload
```

Редагуйте `config.h` і код у [Arduino IDE](<../Arduino IDE>). Файли в `src` підключають відповідні скетчі, тому обидва середовища збирають одні й ті самі вихідні файли. Під час копіювання проєкту збережіть обидві папки поруч.

Для окремої Home-версії редагуйте скетчі у [Home dev internet/2_Carriages_WebSite](<../Home dev internet/2_Carriages_WebSite>). Першу прошивку кожної плати зробіть через `*_serial`. Потім для Wi-Fi оновлення виберіть відповідний `*_ota` і натисніть **Upload**. Мережа `Motor-Control` має існувати окремо, працювати на 2,4 ГГц і фіксованому каналі 6. Якщо ім'я `motor.local` чи `motor-carriage-2.local` не знаходиться, замініть `upload_port` в `platformio.ini` на фактичну IP-адресу плати. Деталі й вимоги до OTA є в README Home-версії.
