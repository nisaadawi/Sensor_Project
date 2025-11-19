const int PULSE_PIN = 36;

int Signal;
int Threshold = 2000;

unsigned long lastBeat = 0;
unsigned long beatTimes[10];     // Last 10 RR intervals
int beatIndex = 0;
int beatsCollected = 0;          // Valid RR intervals
bool rising = false;

// Rolling HRV arrays for median
float rollingRMSSD[5] = {0};
float rollingSDNN[5] = {0};
int rollingIndex = 0;

// ===============================
// Setup
// ===============================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("BPM + Rolling Median HRV Starting...");

  for (int i = 0; i < 10; i++) beatTimes[i] = 0;
}

// ===============================
// Main loop
// ===============================
void loop() {

  Signal = analogRead(PULSE_PIN);

  // Adaptive threshold
  Threshold = (Threshold * 9 + Signal) / 10;

  // Detect rising edge
  if (Signal > Threshold + 25 && !rising) rising = true;

  // Detect falling edge = heartbeat
  if (Signal < Threshold && rising) {
    unsigned long now = millis();
    unsigned long interval = now - lastBeat;

    // Only realistic RR intervals (50–170 BPM)
    if (interval > 350 && interval < 1200) {

      // Optional smoothing with previous interval
      if (beatsCollected > 0) {
        int prevIndex = (beatIndex - 1 + 10) % 10;
        interval = (interval + beatTimes[prevIndex]) / 2;
      }

      // Store interval in circular buffer
      beatTimes[beatIndex] = interval;
      beatIndex = (beatIndex + 1) % 10;
      if (beatsCollected < 10) beatsCollected++;

      // ===== Calculate BPM =====
      long sumInterval = 0;
      for (int i = 0; i < beatsCollected; i++) sumInterval += beatTimes[i];
      long avgInterval = sumInterval / beatsCollected;
      int bpm = 60000 / avgInterval;

      Serial.print("BPM:");
      Serial.print(bpm);

      // ===== Calculate HRV if we have enough beats =====
      if (beatsCollected == 10) {
        float sdnn = computeSDNN();
        float rmssd = computeRMSSD();

        // Update rolling median arrays
        rollingSDNN[rollingIndex] = sdnn;
        rollingRMSSD[rollingIndex] = rmssd;
        rollingIndex = (rollingIndex + 1) % 5;

        // Compute median HRV
        float medianSDNN = median(rollingSDNN, 5);
        float medianRMSSD = median(rollingRMSSD, 5);

        Serial.print("   SDNN(median):");
        Serial.print(medianSDNN);
        Serial.print("   RMSSD(median):");
        Serial.println(medianRMSSD);

      } else {
        Serial.print("   Collecting HRV data...");
        Serial.println(beatsCollected);
      }
    }

    lastBeat = now;
    rising = false;
  }

  delay(10);
}

// ===============================
// HRV Functions
// ===============================

float computeSDNN() {
  float sum = 0, sumSq = 0;
  int count = beatsCollected;
  if (count < 2) return 0;
  for (int i = 0; i < count; i++) {
    float x = (float)beatTimes[i];
    sum += x;
    sumSq += x * x;
  }
  float mean = sum / count;
  return sqrt((sumSq / count) - (mean * mean));
}

float computeRMSSD() {
  float sumSq = 0;
  int count = 0;
  if (beatsCollected < 2) return 0;
  for (int i = 1; i < beatsCollected; i++) {
    int prevIndex = (beatIndex - beatsCollected + i - 1 + 10) % 10;
    int currIndex = (beatIndex - beatsCollected + i + 10) % 10;
    float diff = (float)beatTimes[currIndex] - (float)beatTimes[prevIndex];
    sumSq += diff * diff;
    count++;
  }
  if (count < 1) return 0;
  return sqrt(sumSq / count);
}

// Compute median
float median(float arr[], int size) {
  float temp[size];
  memcpy(temp, arr, sizeof(float) * size);

  // Simple bubble sort
  for (int i = 0; i < size-1; i++)
    for (int j = i+1; j < size; j++)
      if (temp[i] > temp[j]) {
        float t = temp[i]; temp[i] = temp[j]; temp[j] = t;
      }

  if (size % 2 == 0) return (temp[size/2 - 1] + temp[size/2]) / 2;
  else return temp[size/2];
}
