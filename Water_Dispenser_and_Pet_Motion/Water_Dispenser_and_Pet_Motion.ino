#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin Definitions
#define WATER_SENSOR_PIN 36
#define PIR_SENSOR_PIN 13
#define TRIG_PIN 5
#define ECHO_PIN 18
#define RELAY_PIN 25
#define LED_PIN 2

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Configuration
const char* ssid = "Wereen z";
const char* password = "wereenzz";

// Server Backends
const char* logPetURL = "https://humancc.site/ndhos/Pet_Water_Dispenser/log_pet.php";
const char* waterLogURL = "https://humancc.site/ndhos/Pet_Water_Dispenser/log_water.php";

// Thresholds
const int lowDistanceThreshold = 11; // cm
const int lowWaterLevelThreshold = 1000; // analog value
const int highWaterLevelThreshold = 1300; // analog value
const int highDistanceThreshold = 6; // cm

// Timing
unsigned long lastSensorCheck = 0;
const unsigned long sensorInterval = 10000;

// Initial Relay status tracking
String currentRelayStatus = "OFF";

void setup() {
  Serial.begin(115200);

  // Pin Modes
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF initially
  digitalWrite(LED_PIN, LOW);    // LED OFF initially

  // Initialize OLED
  Wire.begin(21, 22); // SDA, SCL
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  analogSetPinAttenuation(36, ADC_11db); // for full 0-3.3V range
}

void loop() {
  unsigned long currentMillis = millis();

  // Auto-reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi!");
    } else {
      Serial.println("\nFailed to reconnect.");
      return;
    }
  }

  // Combined 10s Sensor Read 
  if (currentMillis - lastSensorCheck >= sensorInterval) {
    lastSensorCheck = currentMillis;

    // PIR Sensor Read
    String petStatus, ledStatus;
    if (digitalRead(PIR_SENSOR_PIN) == HIGH) {
      Serial.println("PIR: Pet detected");
      digitalWrite(LED_PIN, HIGH);  // Turn LED ON
      petStatus = "PetDetected";
      ledStatus = "ON";
    } else {
      Serial.println("PIR: No pet detected");
      digitalWrite(LED_PIN, LOW);   // Turn LED OFF
      petStatus = "NoPet";
      ledStatus = "OFF";
    }
    logPetPresence(petStatus, ledStatus);

    // Water Sensor Read
    int waterLevel = analogRead(WATER_SENSOR_PIN);
    long distance = readUltrasonicDistance();
    // Convert distance value to percentage
    int waterPercentage = calculateWaterPercentage(distance);
    String status = "";

    Serial.printf("Water Level: %d, Distance: %ld cm, Percentage: %d%%\n", waterLevel, distance, waterPercentage);

    // Update OLED Display
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Water Level:");
    display.setCursor(0,16);
    display.print(waterPercentage);
    display.println("%");
    display.setCursor(0,32);
    display.print("Distance: ");
    display.print(distance);
    display.println(" cm");
    display.display();

    // Conditions for Water sensor
    if (waterLevel < lowWaterLevelThreshold && distance >= lowDistanceThreshold) {
      // Empty
      digitalWrite(RELAY_PIN, LOW);
      currentRelayStatus = "ON";
      status = "Empty";
      Serial.println(currentRelayStatus);
      Serial.println("Status: Empty, Relay ON");
    } else if (waterLevel < lowWaterLevelThreshold) {
      // Low
      digitalWrite(RELAY_PIN, LOW);
      currentRelayStatus = "ON";
      status = "Low";
      Serial.println(currentRelayStatus);
      Serial.println("Status: Low, Relay ON");
    } else if (waterLevel >= lowWaterLevelThreshold && waterLevel <= highWaterLevelThreshold) {
      // Normal
      digitalWrite(RELAY_PIN, HIGH);
      currentRelayStatus = "OFF";
      status = "Normal";
      Serial.println("Status: Normal, Relay OFF");
    } else if (waterLevel > highWaterLevelThreshold && distance < highDistanceThreshold) {
      // Overflow
      digitalWrite(RELAY_PIN, HIGH);
      currentRelayStatus = "OFF";
      status = "Overflow";
      Serial.println("Status: Overflow, Relay OFF");
    } else {
      status = "Unknown";
      digitalWrite(RELAY_PIN, HIGH);
      currentRelayStatus = "OFF";
    }

    if (status != "") {
      logWaterStatus(status, waterLevel, waterPercentage, currentRelayStatus);
    }
  }
}

// Ultrasonic Distance
long readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  return pulseIn(ECHO_PIN, HIGH) * 0.034 / 2;
}

// Calculate Water Percentage
int calculateWaterPercentage(long distance) {
  if (distance <= highDistanceThreshold) return 100;
  if (distance >= lowDistanceThreshold) return 0;
  return map(distance, highDistanceThreshold, lowDistanceThreshold, 100, 0);
}

// Send Log Pet Detection (with status)
void logPetPresence(String petStatus, String ledStatus) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String url = String(logPetURL) + "?pet_status=" + petStatus +
               "&led_status=" + ledStatus;
  https.begin(client, url);
  int httpCode = https.GET();

  if (httpCode > 0) {
    Serial.println("Pet log sent!");
  } else {
    Serial.println("Failed to log pet: " + https.errorToString(httpCode));
  }
}

// Log Water Status
void logWaterStatus(String status, int waterLevel, int waterPercentage, String currentRelayStatus) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String url = String(waterLogURL) + "?water_level=" + String(waterLevel) +
               "&water_percentage=" + String(waterPercentage) +
               "&water_status=" + status +
               "&relay_status=" + currentRelayStatus;
  https.begin(client, url);
  int httpCode = https.GET();

  if (httpCode > 0) {
    Serial.println("Water status logged!");
    Serial.println(https.getString());
  } else {
    Serial.println("Failed to log water: " + https.errorToString(httpCode));
  }
  https.end();
}
