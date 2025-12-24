#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include "mbedtls/sha256.h"

// RFID pins
#define SS_PIN   4
#define RST_PIN  21
#define SCK_PIN  18
#define MOSI_PIN 23
#define MISO_PIN 19

#define LED_PIN  2

// WiFi credentials
const char* WIFI_SSID = "Wereen z";
const char* WIFI_PASS = "wereenzz";

// PHP API endpoint
const char* API_URL = "https://hfsha.com/IDuCation-web/api_backend/insert_identity.php";

// Must match PHP logic
const char* SECRET_SALT = "IDuCation_2025";

MFRC522 mfrc522(SS_PIN, RST_PIN);

String hashUID(String uid) {
  String input = uid + SECRET_SALT;

  byte hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(
    &ctx,
    (const unsigned char*)input.c_str(),
    input.length()
  );
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  String out = "";
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 0x10) out += "0";
    out += String(hash[i], HEX);
  }
  out.toUpperCase();
  return out;
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // RFID
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();

  Serial.println("Tap RFID card...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  digitalWrite(LED_PIN, HIGH);

  // Read UID
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  String hashedUID = hashUID(uid);

  Serial.println("UID        : " + uid);
  Serial.println("HASHED UID : " + hashedUID);

  // Send to server
  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "hash_uid=" + hashedUID;
  int httpCode = http.POST(postData);

  Serial.print("Server response: ");
  Serial.println(http.getString());

  http.end();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(2000);
}
