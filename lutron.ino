// The file secrets.h should contain the following with the appropriate substitutions...
// #define WIFI_SSID "YOUR_SSID"
// #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
// #define REPEATER_USERNAME "YOUR_REPEATER_USERNAME"
// #define REPEATER_PASSWORD "YOUR_REPEATER_PASSWORD"
// #define REPEATER_IP "YOUR_REPEATER_IP"
// #define REPEATER_PORT 23
#include <WiFi.h>
#include "secrets.h"

WiFiClient client;

struct Button {
  int integrationId; // Lutron integration ID
  int number;
  String name;
};

struct SwitchControl {
  int pin; // Arduino digital input #
  String name;
  Button button;
  bool state;
};

SwitchControl switchControls[] = {
  {23, "Office", {57, 4, "Sconces and Sideboard Lamps"}, false},
  {25, "Living Room", {57, 1, "Floor Lamps"}, false},
  {27, "Door (Left)", {57, 5, "Exterior Lights"}, false},
  {29, "Door (Middle Left)", {57, 5, "Exterior Lights"}, false},
  {31, "Door (Middle Right)", {57, 5, "Exterior Lights"}, false},
  {33, "Door (Right)", {57, 10, "Hall 1 Pendant"}, false},
  {35, "Stairs (Left)", {34, 1, "Landing Sconce and Hall 2 Pendant"}, false},
  {37, "Stairs (Right)", {57, 10, "Hall 1 Pendant"}, false},
  {39, "Hall (Left)", {57, 10, "Hall 1 Pendant"}, false},
  {41, "Hall (Right)",  {58, 1, "Kitchen"}, false},
  {43, "Dining Room (Left)", {43, 4, "Dining Room Sconces and Chandelier"}, false},
  {45, "Dining Room (Right)", {43, 5, "Dining Room Sconces and Chandelier"}, false},
  {47, "Kitchen (Left)", {58, 1, "Kitchen"}, false},
  {49, "Kitchen (Middle Left)", {58, 1, "Kitchen"}, false},
  {51, "Kitchen (Middle Right)", {58, 1, "Kitchen"}, false},
  {53, "Kitchen (Right)", {58, 1, "Kitchen"}, false}
};

const int NUM_SWITCHES = sizeof(switchControls) / sizeof(switchControls[0]);
const unsigned long RETRY_DELAY = 30 * 1000;
const unsigned long WIFI_TIMEOUT = 30 * 1000;
const unsigned long PROMPT_TIMEOUT = 30 * 1000;
const unsigned long SWITCH_STATE_CHECK_DELAY = 50;
const unsigned long RECONNECT_AFTER = 30 * 1000;

unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  // while (!Serial); // Wait for native USB port to connect
  while(!connectWiFi() || !connectToRadioRA2()) {
    serialPrintln("Retrying connection...");
    delay(RETRY_DELAY);
  }; // do nothing endlessly
  setPinMode();
  initializeSwitchState();
}

void serialPrint()

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime >= SWITCH_STATE_CHECK_DELAY) {
    lastDebounceTime = currentTime;
    reconnectIfNeeded();
    handleSwitchStateChanges();
  }
  if (client.available()) { // Keep getting responses from Lutron Main Repeater (keep alive)
    char c = client.read();
  }
}

void serialPrint(const String& message) {
  if (Serial) {
    Serial.print(message);
  }
}

void serialPrintln(const String& message) {
  if (Serial) {
    Serial.println(message);
  }
}

bool connectWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    serialPrintln("Communication with WiFi module failed!");
    return false;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  serialPrint("Connecting to WiFi...");
  unsigned long wifiStartTime = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiStartTime >= WIFI_TIMEOUT) {
      serialPrintln("\nTimeout while connecting to WiFi.");
      return false;
    }
    delay(500);
    serialPrint(".");
  }
  serialPrintln("\nConnected to WiFi.");
  return true;
}

bool waitForPrompt(const char* prompt) {
  String response = "";
  unsigned long startTime = millis(); // Get the current time at the start of the function

  while (client.connected()) {
    if (millis() - startTime >= PROMPT_TIMEOUT) {
      serialPrintln("Timeout while waiting for " + String(prompt));
      return false;
    }

    while (client.available()) {
      char c = client.read();
      response += c;
      if (response.endsWith(prompt)) {
        return true;
      }
    }
  }
  return false;
}

void setPinMode() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    pinMode(switchControls[i].pin, INPUT_PULLUP);
  }
  delay(100); // wait for board resistors to stabilize to prevent floating inputs
}

void initializeSwitchState() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    switchControls[i].state = (digitalRead(switchControls[i].pin) == LOW);
  }
}

bool connectToRadioRA2() {
  serialPrintln("Connecting to Lutron RadioRA2 Main Repeater...");
  if (!client.connect(REPEATER_IP, REPEATER_PORT)) {
    serialPrintln("Connection to Lutron RadioRA2 Main Repeater failed.");
    return false;
  }

  if (waitForPrompt("login: ")) {
    client.println(REPEATER_USERNAME);
  } else {
    serialPrintln("Failed to receive login prompt.");
    return false;
  }

  if (waitForPrompt("password: ")) {
    client.println(REPEATER_PASSWORD);
  } else {
    serialPrintln("Failed to receive password prompt.");
    return false;
  }

  if (!waitForPrompt("GNET>")) {
    serialPrintln("Failed to login.");
    return false;
  }
  serialPrintln("Connected to Lutron RadioRA2 Main Repeater.");
  return true;
}

void reconnectIfNeeded() {
  static unsigned long lastReconnectAttempt = 0;  // Keep track of last attempt time

  if (!client.connected()) {
    unsigned long currentTime = millis();

    // Try immediately if it's the first attempt or the reconnect interval has passed
    if (lastReconnectAttempt == 0 || (currentTime - lastReconnectAttempt >= RECONNECT_AFTER)) {
      lastReconnectAttempt = currentTime;  // Update the last attempt time
      client.stop();
      serialPrintln("Lost connection. Attempting to reconnect to Lutron RadioRA2 Main Repeater...");

      if (connectToRadioRA2()) {
        serialPrintln("Reconnected to Lutron RadioRA2 Main Repeater.");
        lastReconnectAttempt = 0;  // Reset last attempt time after a successful connection
      } else {
        serialPrintln("Reconnect attempt failed.");
        NVIC_SystemReset();
      }
    }
  }
}

void pressButton(Button button) {
  char command[50];
  sprintf(command, "#DEVICE,%d,%d, 3", button.integrationId, button.number);
  client.println(command);
  if (waitForPrompt("GNET>")) {
    serialPrintln("SCENE: " + button.name + ".");
  } else {
    serialPrintln("Lutron Main Repeater did not respond with GNET> prompt.");
  }
}

void handleSwitchStateChanges() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    bool currentState = (digitalRead(switchControls[i].pin) == LOW);
    if (currentState != switchControls[i].state) {
      switchControls[i].state = currentState;
      serialPrintln("SWITCH: " + switchControls[i].name + " turned " + (currentState ? "on.": "off."));
      pressButton(switchControls[i].button);
    }
  }
}
