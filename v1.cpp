#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Keypad.h>

// OLED
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 9, 8);  // SCL = 9, SDA = 8

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {42, 41, 40, 39}; // Row
byte colPins[COLS] = {38, 37, 36, 35}; // Col
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// MAC Address
uint8_t peerAddress[] = {0xA0, 0x85, 0xE3, 0xF0, 0x8F, 0x18};

String messageBuffer = "";

// Callback
void onReceive(const uint8_t * mac, const uint8_t *incomingData, int len) {
  String msg = String((char*)incomingData);
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 10, "Received:");
  display.drawStr(0, 30, msg.c_str());
  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);

  // Initialize display
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 10, "Booting...");
  display.sendBuffer();

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    display.clearBuffer();
    display.drawStr(0, 10, "ESP-NOW Init Failed");
    display.sendBuffer();
    return;
  }

  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(peerAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      display.clearBuffer();
      display.drawStr(0, 10, "Add Peer Failed");
      display.sendBuffer();
      return;
    }
  }

  display.clearBuffer();
  display.drawStr(0, 10, "Ready to type");
  display.sendBuffer();
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == '#') {
      // Send message
      if (messageBuffer.length() > 0) {
        esp_now_send(peerAddress, (uint8_t *)messageBuffer.c_str(), messageBuffer.length() + 1);

        display.clearBuffer();
        display.drawStr(0, 10, "Sent:");
        display.drawStr(0, 30, messageBuffer.c_str());
        display.sendBuffer();

        Serial.println("Message sent: " + messageBuffer);
        messageBuffer = "";
        delay(1000);

        display.clearBuffer();
        display.drawStr(0, 10, "Ready to type");
        display.sendBuffer();
      }
    } else if (key == '*') {
      // Backspace
      if (messageBuffer.length() > 0) {
        messageBuffer.remove(messageBuffer.length() - 1);
      }
    } else {
      // Add character
      messageBuffer += key;
    }

    // Show what's being typed
    display.clearBuffer();
    display.drawStr(0, 10, "Typing:");
    display.drawStr(0, 30, messageBuffer.c_str());
    display.sendBuffer();
  }
}
