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

// Preferences
Preferences prefs;
int messageCount = 0;
int historyIndex = 0;

// Mode
String messageBuffer = "";
bool isTypingMode = true;  // Start in typing mode

// ESP-NOW
uint8_t peerAddress[] = {0xA0, 0x85, 0xE3, 0xF0, 0x8F, 0x18};
int shift = 3;

// Characters
const char characterSet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
const int numCharacters = sizeof(characterSet) - 1; // Exclude null terminator
int currentCharIndex = 0;
int lastStableCharIndex = -1;

// Potentiometer Pin
const int potPin = 1;  // Use GPIO1

// Dummy Slots Between Characters
const int extraSlotsBetween = 3; // number of dummy slots between real chars
const int totalSlots = numCharacters * (extraSlotsBetween + 1) - extraSlotsBetween;

// Caesar Cipher
String encrypt(String message) {
  String result = "";
  for (char c : message) {
    if (isAlpha(c)) {
      result += char((c - 'A' + shift) % 26 + 'A');
    } else if (isdigit(c)) {
      result += char((c - '0' + shift) % 10 + '0');
    } else if (c == ' ') {
      result += ' ';
    } else {
      result += c;
    }
  }
  return result;
}

String decrypt(String message) {
  String result = "";
  for (char c : message) {
    if (isAlpha(c)) {
      result += char((c - 'A' - shift + 26) % 26 + 'A');
    } else if (isdigit(c)) {
      result += char((c - '0' - shift + 10) % 10 + '0');
    } else if (c == ' ') {
      result += ' ';
    } else {
      result += c;
    }
  }
  return result;
}

bool isAlpha(char c) {
  return c >= 'A' && c <= 'Z';
}

// Message History
void saveMessage(String msg, String type) {
  prefs.begin("messages", false);
  prefs.putString(("msg" + String(messageCount)).c_str(), type + ": " + msg);
  messageCount++;
  prefs.putInt("count", messageCount);
  prefs.end();
}

String loadMessage(int index) {
  prefs.begin("messages", true);
  String msg = prefs.getString(("msg" + String(index)).c_str(), "");
  prefs.end();
  return msg;
}

// ESP-NOW Receive
void onReceive(const uint8_t *mac, const uint8_t *incomingData, int len) {
  String encryptedMsg = String((char*)incomingData);
  String msg = decrypt(encryptedMsg);
  saveMessage(msg, "Received");

  if (isTypingMode) {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(0, 10, "Received:");
    display.drawStr(0, 30, msg.c_str());
    display.sendBuffer();
  }
}

// Setup
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
    esp_now_add_peer(&peerInfo);
  }

  display.clearBuffer();
  display.drawStr(0, 10, "Ready to type");
  display.sendBuffer();
}

// Loop
void loop() {
  char key = keypad.getKey();

  // Potentiometer Reading
  int potVal = 0;
  for (int i = 0; i < 10; i++) {
    potVal += analogRead(potPin);
    delay(1);
  }
  potVal /= 10;

  // Map potVal to totalSlots (including dummy slots)
  int virtualIndex = map(potVal, 0, 4095, 0, totalSlots - 1);

  // Calculate actual char index by ignoring dummy slots
  int charIndex = virtualIndex / (extraSlotsBetween + 1);
  int remainder = virtualIndex % (extraSlotsBetween + 1);

  // Update display only when we are on a real character slot (remainder == 0)
  if (remainder == 0 && charIndex != lastStableCharIndex) {
    currentCharIndex = charIndex;
    lastStableCharIndex = currentCharIndex;
  }

  if (key) {
    if (key == 'D') {
      isTypingMode = !isTypingMode;
      historyIndex = 0;
      delay(300); // delay
    }

    if (isTypingMode) {
      if (key == '#') {
        if (messageBuffer.length() > 0) {
          String encrypted = encrypt(messageBuffer);
          esp_now_send(peerAddress, (uint8_t *)encrypted.c_str(), encrypted.length() + 1);
          saveMessage(messageBuffer, "Sent");

          display.clearBuffer();
          display.drawStr(0, 10, "Sent:");
          display.drawStr(0, 30, messageBuffer.c_str());
          display.sendBuffer();
          messageBuffer = "";
          delay(1000);
        }
      } else if (key == '*') {
        if (messageBuffer.length() > 0) messageBuffer.remove(messageBuffer.length() - 1);
      } else if (key == 'C') {
        messageBuffer = "";
        display.clearBuffer();
        display.drawStr(0, 10, "Typing Cleared");
        display.sendBuffer();
        delay(500);
      } else if (key == '0') {
        messageBuffer += characterSet[currentCharIndex];
      }
    } else {
      // History mode keys
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
        delay(1000);
        return;
      }
    }
  }

  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tr);

  if (isTypingMode) {
    display.drawStr(0, 10, "Typing:");
    display.drawStr(50, 10, messageBuffer.c_str());

    display.drawStr(0, 30, "Select:");
    String shownChar = characterSet[currentCharIndex] == ' ' ? "[SPACE]" : String(characterSet[currentCharIndex]);
    display.drawStr(50, 30, shownChar.c_str());
  } else {
    display.drawStr(0, 10, "History:");
    if (messageCount == 0) {
      display.drawStr(0, 30, "No messages");
    } else {
      String histMsg = loadMessage(historyIndex);
      String idxStr = String(historyIndex + 1) + "/" + String(messageCount);
      display.drawStr(0, 20, idxStr.c_str());
      display.drawStr(0, 40, histMsg.c_str());
    }
  }

  display.sendBuffer();
}
