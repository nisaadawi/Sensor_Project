const int PULSE_PIN = 36;
int Signal;
int Threshold = 2000; // Adaptive threshold starting point

unsigned long lastBeat = 0;
unsigned long beatTimes[10];
int beatIndex = 0;
bool rising = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Accurate Finger BPM starting...");
}

void loop() {
  Signal = analogRead(PULSE_PIN);

  static int prev = Signal;

  // Adaptive threshold movement
  Threshold = (Threshold * 9 + Signal) / 10;

  if (Signal > Threshold + 25 && !rising) {
    rising = true;
  }

  if (Signal < Threshold && rising) {
    unsigned long now = millis();
    unsigned long interval = now - lastBeat;

    if (interval > 350 && interval < 2000) { // Only valid heart intervals
      beatTimes[beatIndex] = interval;
      beatIndex = (beatIndex + 1) % 10;

      long avgInterval = 0;
      for (int i = 0; i < 10; i++) {
        avgInterval += beatTimes[i];
      }
      avgInterval /= 10;

      int bpm = 60000 / avgInterval;
      Serial.println(bpm);   // Only print the number

    }

    lastBeat = now;
    rising = false;
  }

  prev = Signal;
  delay(10);
}