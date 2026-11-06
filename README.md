# 12GAN20-20 — система керування каретками

Усі актуальні Arduino IDE-скетчі та локальні демо знаходяться в папці [12GAN20-20](12GAN20-20).

```text
12GAN20-20/
  1_Carriage_WebSite/
    NodeMCU/
    ESP32S3/
    Wemos/
  2_Carriages_WebSite/
    ESP32_Controller_Carriage_1/
    NodeMCU_Carriage_2/
  web-interface-demo.html
  web-interface-two-carriages-demo.html
```

## Який скетч прошивати

Для однієї каретки із сайтом оберіть одну з трьох платформ у `1_Carriage_WebSite`:

- `NodeMCU/NodeMCU.ino` — NodeMCU v3;
- `ESP32S3/ESP32S3.ino` — ESP32-S3;
- `Wemos/Wemos.ino` — Wemos D1 mini Pro.

Для двох кареток:

- `2_Carriages_WebSite/ESP32_Controller_Carriage_1/ESP32_Controller_Carriage_1.ino` — ESP32 із сайтом і керуванням першою кареткою;
- `2_Carriages_WebSite/NodeMCU_Carriage_2/NodeMCU_Carriage_2.ino` — NodeMCU другої каретки.

Локальні HTML-файли — це лише візуальні демо для браузера, вони не керують реальною платою.

Wi-Fi для актуальних скетчів: `Motor-Control`, пароль `motor8266`, адреса `http://192.168.4.1`.

Робоча площа троса: 16 м, орієнтовний діапазон від центру `−8…+8 м`. Власні вимірювання швидкості потрібно внести у `config.h` відповідного скетчу.

Для початкового вимірювання часу відкрийте на сайті «Ручне калібрування». Кнопки `1 с`, `2 с`, `3 с`, `4 с` запускають автоматичну зупинку після вибраного часу. Реальну відстань виміряйте рулеткою та внесіть окремо для вправо/вліво у `TIME_CALIBRATION` відповідного `config.h`.

Логіка програм: «Маятник» їде від −8 до +8 м; «Драбинка» зупиняється на −2, +2, −4, +4, −6, +6, −8, +8 м; «Ривки» роблять хаотичні переміщення на 1–3 м з максимальною потужністю; «Рандом» вибирає довші випадкові переходи приблизно від 4 м.

Детальні схеми: [README_STANDALONE_LED.md](12GAN20-20/README_STANDALONE_LED.md) і [README_TWO_CARRIAGES.md](12GAN20-20/README_TWO_CARRIAGES.md).
