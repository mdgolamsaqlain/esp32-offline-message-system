#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Keypad.h>
#include <Preferences.h>

// OLED Setup
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 9, 8);  // SCL = 9, SDA = 8

// Keypad Setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {42, 41, 40, 39};
byte colPins[COLS] = {38, 37, 36, 35};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Receiver MAC Address
uint8_t peerAddress[] = {0xA0, 0x85, 0xE3, 0xF0, 0x8F, 0x18};

// Message History
Preferences prefs;
int messageCount = 0;
int historyIndex = 0;

// Mode and Buffer
String messageBuffer = "";
bool isTypingMode = true;

void saveMessage(String msg);
String loadMessage(int index);

// ESP-NOW Receive Callback
void onReceive(const uint8_t *mac, const uint8_t *incomingData, int len) {
  String msg = String((char*)incomingData);
  saveMessage(msg);

  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 10, "Received:");
  display.drawStr(0, 30, msg.c_str());
  display.sendBuffer();
}

// Save Message to Preferences
void saveMessage(String msg) {
  prefs.begin("messages", false);
  prefs.putString(("msg" + String(messageCount)).c_str(), msg.c_str());
  messageCount++;
  prefs.putInt("count", messageCount);
  prefs.end();
}

// Load Message from History
String loadMessage(int index) {
  prefs.begin("messages", true);
  String msg = prefs.getString(("msg" + String(index)).c_str(), "");
  prefs.end();
  return msg;
}

void setup() {
  Serial.begin(115200);

  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);
  display.drawStr(0, 10, "Booting...");
  display.sendBuffer();

  prefs.begin("messages", true);
  messageCount = prefs.getInt("count", 0);
  prefs.end();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
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
    Serial.print("Key pressed: ");
    Serial.println(key);

    if (key == 'D') {
      isTypingMode = !isTypingMode;
      historyIndex = 0;
      display.clearBuffer();
      display.setFont(u8g2_font_6x10_tr);
      display.drawStr(0, 10, isTypingMode ? "Ready to type" : "History Mode");
      display.sendBuffer();
      delay(300);  // delay
      return;
    }

    if (isTypingMode) {
      if (key == '#') {
        if (messageBuffer.length() > 0) {
          esp_now_send(peerAddress, (uint8_t *)messageBuffer.c_str(), messageBuffer.length() + 1);
          saveMessage(messageBuffer);

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
        if (messageBuffer.length() > 0) {
          messageBuffer.remove(messageBuffer.length() - 1);
        }
      } else if (key == 'C') {
        messageBuffer = "";
        display.clearBuffer();
        display.drawStr(0, 10, "Typing Cleared");
        display.sendBuffer();
      } else {
        messageBuffer += key;
      }

      display.clearBuffer();
      display.drawStr(0, 10, "Typing:");
      display.drawStr(0, 30, messageBuffer.c_str());
      display.sendBuffer();
    } else {
      if (key == 'A') {
        if (historyIndex > 0) historyIndex--;
      } else if (key == 'B') {
        if (historyIndex < messageCount - 1) historyIndex++;
      } else if (key == 'C') {
        prefs.begin("messages", false);
        prefs.clear();
        prefs.end();
        messageCount = 0;
        historyIndex = 0;
        display.clearBuffer();
        display.drawStr(0, 10, "History Cleared");
        display.sendBuffer();
        return;
      }

      if (messageCount > 0) {
        String msg = loadMessage(historyIndex);
        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(0, 10, "History:");
        display.drawStr(0, 30, msg.c_str());
        display.sendBuffer();
      }
    }
  }
}
