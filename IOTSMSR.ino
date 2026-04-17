#include <WiFi.h>
#include "ThingSpeak.h"

// --- Wi-Fi Settings ---
const char* ssid = "OPPO F25 Pro 5G";
const char* password = "7338855384";

// --- ThingSpeak Settings ---
unsigned long myChannelNumber = 3136631;
const char * myWriteAPIKey = "DAC1V1Y5ZNFNZ7PQ";

// --- Pin Definitions & Settings ---
const int ldrPin = 34;
const int mq3_pin = 35;     // Spoilage
const int mq2_pin = 32;     // Gas Leak
const int greenLedPin = 4;
const int redLedPin = 2;
const int buzzerPin = 27;

// --- Thresholds (Using your test values) ---
const int DOOR_THRESHOLD = 1000;
const int SPOILAGE_THRESHOLD = 2000; 
const int GAS_THRESHOLD = 1000;      
const long DOOR_OPEN_INTERVAL = 30000; // 30 seconds

// --- Global Variables ---
unsigned long doorOpenStartTime = 0;
int alarmState = 0; // 0=OK, 1=Door, 2=Spoilage, 3=Gas Leak
WiFiClient client;

void setup() {
  Serial.begin(115200);
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(greenLedPin, HIGH);
  digitalWrite(redLedPin, LOW);
  noTone(buzzerPin);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected!");
  
  ThingSpeak.begin(client);
  Serial.println("Smart Fridge Monitor: ONLINE");
}

void loop() {
  // 1. Read all sensor values
  int ldrValue = analogRead(ldrPin);
  int spoilageValue = analogRead(mq3_pin);
  int mq2Value = analogRead(mq2_pin);

  // Print all sensor data for monitoring
  Serial.print("LDR: ");
  Serial.print(ldrValue);
  Serial.print(" | Spoilage (MQ-3): ");
  Serial.print(spoilageValue);
  Serial.print(" | Gas (MQ-2): ");
  Serial.println(mq2Value);

  // 2. Determine all alarm conditions
  bool doorOpenTooLong = false;
  if (ldrValue > DOOR_THRESHOLD) {
    if (doorOpenStartTime == 0) { doorOpenStartTime = millis(); }
    if (millis() - doorOpenStartTime > DOOR_OPEN_INTERVAL) {
      doorOpenTooLong = true;
    }
  } else {
    doorOpenStartTime = 0;
  }
  
  bool spoilageDetected = (spoilageValue > SPOILAGE_THRESHOLD);
  bool gasLeakDetected = (mq2Value > GAS_THRESHOLD);

  // 3. --- Prioritized Alarm Trigger Logic ---
  if (gasLeakDetected) {
    if (alarmState != 3) {
      Serial.println("!!! ALARM: Gas Leak Detected !!!");
      setAlarmState(3);
    }
  } 
  else if (spoilageDetected) {
    if (alarmState != 2) { 
      Serial.println("!!! ALARM: Spoilage Detected !!!");
      setAlarmState(2);
    }
  } 
  else if (doorOpenTooLong) {
    if (alarmState != 1) {
      Serial.println("!!! ALARM: Door Open Too Long !!!");
      setAlarmState(1);
    }
  } 
  else {
    if (alarmState != 0) {
      Serial.println("System OK. Alarm reset.");
      setAlarmState(0);
    }
  }
  
  // Regular update to ThingSpeak
  static unsigned long lastUpdateTime = 0;
  if (alarmState == 0 && (millis() - lastUpdateTime > 60000)) {
    Serial.println("Sending regular status update...");
    sendDataToThingSpeak(ldrValue, spoilageValue, mq2Value, 0);
    lastUpdateTime = millis();
  }
  
  delay(500);
}

// --- Helper function to manage alarm state and send data ---
void setAlarmState(int state) {
  alarmState = state;
  int ldr = analogRead(ldrPin);
  int spoilage = analogRead(mq3_pin);
  int gas = analogRead(mq2_pin);
  
  if (state == 0) { // System OK
    digitalWrite(greenLedPin, HIGH);
    digitalWrite(redLedPin, LOW);
    noTone(buzzerPin);
  } else { // Any Alarm
    digitalWrite(greenLedPin, LOW);
    digitalWrite(redLedPin, HIGH);
    tone(buzzerPin, 1000);
  }
  
  // Send the new state to ThingSpeak immediately
  sendDataToThingSpeak(ldr, spoilage, gas, state);
}

// --- Helper function to send data ---
void sendDataToThingSpeak(int ldr, int spoilage, int gas, int alarm) {
  ThingSpeak.setField(1, ldr);
  ThingSpeak.setField(2, spoilage);
  ThingSpeak.setField(3, alarm);
  ThingSpeak.setField(4, gas);
  
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
}