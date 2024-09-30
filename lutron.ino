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
  {2, "Office", {57, 4, "Sconces and Sideboard Lamps"}, false},
  {3, "Living Room", {57, 1, "Floor Lamps"}, false},
  {4, "Door (Left)", {57, 5, "Exterior Lights"}, false},
  {5, "Door (Middle Left)", {57, 5, "Exterior Lights"}, false},
  {6, "Door (Middle Right)", {57, 5, "Exterior Lights"}, false},
  {7, "Door (Right)", {57, 10, "Hall 1 Pendant"}, false},
  {8, "Stairs (Left)", {34, 1, "Landing Sconce and Hall 2 Pendant"}, false},
  {9, "Stairs (Right)", {57, 10, "Hall 1 Pendant"}, false},
  {23, "Hall (Left)", {57, 10, "Hall 1 Pendant"}, false},
  {24, "Hall (Right)",  {58, 1, "Kitchen"}, false},
  {25, "Dining Room (Left)", {43, 4, "Dining Room Sconces and Chandelier"}, false},
  {26, "Dining Room (Right)", {43, 5, "Dining Room Sconces and Chandelier"}, false},
  {27, "Kitchen (Left)", {58, 1, "Kitchen"}, false},
  {28, "Kitchen (Middle Left)", {58, 1, "Kitchen"}, false},
  {29, "Kitchen (Middle Right)", {58, 1, "Kitchen"}, false},
  {30, "Kitchen (Right)", {58, 1, "Kitchen"}, false}
};

const int NUM_SWITCHES = sizeof(switchControls) / sizeof(switchControls[0]);
const unsigned long DEBOUNCE_DELAY = 50;
unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for native USB port to connect
  while(!connectWiFi() || !connectToRadioRA2()); // do nothing endlessly
  setPinMode();
  initializeSwitchState();
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime >= DEBOUNCE_DELAY) {
    lastDebounceTime = currentTime;
    reconnectIfNeeded();
    handleSwitchStateChanges();
  }
  if (client.available()) { // Keep outputting responses from Lutron Main Repeater
    char c = client.read();
    Serial.print(c);
  }
}

bool connectWiFi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    return false;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" done.");
  return true;
}

bool waitForPrompt(const char* prompt) {
  String response = "";
  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      response += c;
      Serial.print(c);
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
  Serial.print("Connecting to Lutron RadioRA2 Main Repeater...");
  if (!client.connect(REPEATER_IP, REPEATER_PORT)) {
    Serial.println("Connection failed.");
    return false;
  }
  Serial.println(" done.");

  if (waitForPrompt("login: ")) {
    client.println(REPEATER_USERNAME);
    Serial.println(REPEATER_USERNAME);
  } else {
    Serial.println("Failed to receive login prompt.");
    return false;
  }

  if (waitForPrompt("password: ")) {
    Serial.println("•••••••••");
    client.println(REPEATER_PASSWORD);
  } else {
    Serial.println("Failed to receive password prompt.");
    return false;
  }

  if (!waitForPrompt("GNET> ")) {
    Serial.println("Failed to login.");
    return false;
  }
  Serial.println(" ");
  return true;
}

void reconnectIfNeeded() {
    if (!client.connected()) {
    client.stop();
    Serial.println("Lost connection. Reconnecting to Lutron RadioRA2 Main Repeater.");
    if (!connectToRadioRA2()) {
      Serial.println("Could not reconnect to Lutron RadioRA2 Main Repeater.");
      while (true); // endlessly repeat
    }
  }
}

void pressButton(Button button) {
  char command[50];
  sprintf(command, "#DEVICE,%d,%d, 3", button.integrationId, button.number);
  client.println(command);
  if (waitForPrompt("GNET>")) {
    Serial.println("  Toggled " + button.name + " via " + command);
  }
}

void handleSwitchStateChanges() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    bool currentState = (digitalRead(switchControls[i].pin) == LOW);
    if (currentState != switchControls[i].state) {
      switchControls[i].state = currentState;
      pressButton(switchControls[i].button);
      Serial.println(switchControls[i].name + " turned " + (switchControls[i].state ? "on.": "off."));
    }
  }
}
