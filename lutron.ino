#include <WiFi.h>
#include "secrets.h" 

WiFiClient client;

const int MAX_DEVICES_PER_SWITCH = 2;

struct Device {
  int id;    // Lutron Integration ID
  String name;
  int onLevel; // Dimmer level when on (0-100)
};

struct SwitchControl {
  int pin;   // Arduino digital input #
  String name;
  Device devices[MAX_DEVICES_PER_SWITCH];
  int numberOfDevices;
  bool on;   // Switch state, default to false
};

// Removed 'const' so the array elements can be modified
SwitchControl SWITCH_CONTROLS[] = {
  {2, "Office", {{28, "Office Sconces", 100}, {103, "Office Lamps", 100}}, 2, false},
  {3, "Living Room", {{74, "Living Room Lamps", 75}}, 1, false},
  {4, "Door (Left)", {{31, "Porch Pendant", 75}}, 1, false},
  {5, "Door (Middle Left)", {{32, "Porch Vestibule", 53}}, 1, false},
  {6, "Door (Middle Right)", {{33, "Porch Recessed", 32}}, 1, false},
  {7, "Door (Right)", {{29, "Hall 1 Pendant", 75}}, 1, false},
  {8, "Stairs (Left)", {{38, "Hall 2 Pendant", 75}, {40, "Hall 2 Sconce", 50}}, 2, false},
  {9, "Stairs (Right)", {{29, "Hall 1 Pendant", 75}}, 1, false},
  {23, "Hall (Left)", {{29, "Hall 1 Pendant", 75}}, 1, false},
  {24, "Hall (Right)", {{22, "Dining Room Chandelier", 75}, {64, "Dining Room Sconces", 40}}, 2, false},
  {25, "Dining Room (Left)", {{64, "Dining Room Sconces", 40}}, 1, false},
  {26, "Dining Room (Right)", {{22, "Dining Room Chandelier", 75}}, 1, false},
  {27, "Kitchen (Left)", {{67, "Kitchen Pendants", 100}}, 1, false},
  {28, "Kitchen (Middle Left)", {{62, "Kitchen Recessed Main", 75}, {69, "Kitchen Recessed Pantry", 75}}, 2, false},
  {29, "Kitchen (Middle Right)", {{68, "Kitchen Counters Main", 100}, {70, "Kitchen Counters Pantry", 100}}, 2, false},
  {30, "Kitchen (Right)", {{65, "Kitchen Door", 95}}, 1, false}
};

const int NUM_SWITCHES = sizeof(SWITCH_CONTROLS) / sizeof(SWITCH_CONTROLS[0]);
const unsigned long DEBOUNCE_DELAY = 50;
unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);  // do nothing endlessly
  while(!connectWiFi() || !connectToRadioRA2()); // do nothing endlessly
  setPinMode();
  delay(2000); // wait for board resistors to stabilize to prevent floating inputs
  setupSwitches();
}

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime >= DEBOUNCE_DELAY) {
    lastDebounceTime = currentTime;
    for (int i = 0; i < NUM_SWITCHES; i++) {
      bool currentState = (digitalRead(SWITCH_CONTROLS[i].pin) == LOW); // true if "on"
      if (currentState != SWITCH_CONTROLS[i].on) { // state changed
        SWITCH_CONTROLS[i].on = currentState; // update the on field
        handleSwitch(i, currentState);
      }
    }
  }

  if (!client.connected()) {
    client.stop();
    Serial.println("Disconnected from Lutron RadioRA2 Main Repeater.");
    while(!connectToRadioRA2()); // do nothing endlessly
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
    delay(1000);
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
    pinMode(SWITCH_CONTROLS[i].pin, INPUT_PULLUP);
  }
}

void setupSwitches() {
  for (int i = 0; i < NUM_SWITCHES; i++) {
    SWITCH_CONTROLS[i].on = (digitalRead(SWITCH_CONTROLS[i].pin) == LOW); // true if "on"
    // Serial.println("Switch " + String(SWITCH_CONTROLS[i].pin) + (SWITCH_CONTROLS[i].on ? " on" : " off"));
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

void turnOn(Device device) {
  char command[50];
  sprintf(command, "#OUTPUT,%d,1,%d", device.id, device.onLevel);
  client.println(command);
  Serial.println("  " + device.name + " on.");
}

void turnOff(Device device) {
  char command[50];
  sprintf(command, "#OUTPUT,%d,1,0,00:03", device.id);
  client.println(command);
  Serial.println("  " + device.name + " off.");
}

void handleSwitch(int switchIndex, bool on) {
  SwitchControl currentSwitch = SWITCH_CONTROLS[switchIndex];
  Serial.println(currentSwitch.name + " switch was turned " + (on ? "on." : "off."));

  if (on) {
    for (int i = 0; i < currentSwitch.numberOfDevices; i++) {
      turnOn(currentSwitch.devices[i]);
    }
  } else {
    for (int i = 0; i < currentSwitch.numberOfDevices; i++) {
      turnOff(currentSwitch.devices[i]);
    }
  }
}
