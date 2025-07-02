void setup() {
  Serial.begin(115200);
  analogSetPinAttenuation(36, ADC_11db); // for ESP32, full 0-3.3V range
}

void loop() {
  int value = analogRead(36);
  Serial.println(value);
  delay(500);
}