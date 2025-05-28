#include <WiFi.h>
#include <WiFiClientSecure.h> //for https
#include <HTTPClient.h>
#include "DHT.h"

// DHT11 Configuration
#define DHTPIN 4         // DHT11 sensor connected to GPIO 4
#define DHTTYPE DHT11    // Sensor type

// WiFi credentials
const char* ssid = "Wereen z";
const char* pass = "wereenzz";

// Your server URL
const char* serverName = "https://humancc.site/ndhos/DHT11/dht11_backend.php";

// Variables
float hum = 0, temp = 0;
unsigned long sendDataPrevMillis = 0;
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Connect to WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Send data every 10 seconds
  if (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0) {
    getDHT(); // Read sensor data

    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();  // WARNING: Not secure! For testing only

      HTTPClient https;
      // Construct the GET request URL
      String httpURL = String(serverName) + 
                       "?device_id=101" + 
                       "&temperature=" + String(temp, 2) + 
                       "&humidity=" + String(hum, 2);

      Serial.print("Sending to: ");
      Serial.println(httpURL);

      https.begin(client, httpURL);
      int httpResponse = https.GET();

      if (httpResponse > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponse);
        String payload = https.getString();
        Serial.println("Server response: " + payload);
      } else {
        Serial.print("HTTP Error: ");
        Serial.println(https.errorToString(httpResponse));
      }

      https.end();
    } else {
      Serial.println("WiFi disconnected!");
    }

    sendDataPrevMillis = millis();  // Update last sent time
  }
}

// Read temperature and humidity from DHT11
void getDHT() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.printf("Temperature: %.2f°C\tHumidity: %.2f%%\n", temp, hum);
}
