# IR Blaster FOTA (ESP32-C3 Super Mini)
Kien truc: HTTPS pull OTA (GitHub Releases) kich hoat qua MQTT (EMQX Serverless) tu Node-RED.

1. Sao chep `src/secrets.h.example` thanh `src/secrets.h` va dien thong tin that (file nay da nam trong .gitignore).
2. Mo thu muc bang VS Code + PlatformIO, build/upload.
3. Import `nodered-flow.json` vao Node-RED, sua Device ID / URL trong node "Tao lenh OTA".
4. Phat hanh: tang FW_VERSION trong platformio.ini, build, dinh kem .pio/build/esp32c3_supermini/firmware.bin vao GitHub Release.
