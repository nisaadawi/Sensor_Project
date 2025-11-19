// ============================================
// ESP32 PPG BPM + RMSSD + AC/DC + Stress
// Serial Output Only
// ============================================

#define PPG_PIN 36           // Pulse sensor input
#define FILTER_SIZE 30       // Moving average filter
#define DC_WINDOW 100        // Moving average for baseline
#define MIN_PEAK_HEIGHT 50   // Minimum amplitude for a valid peak
#define MIN_BEAT_INTERVAL 300 // ms, to avoid double counting

// --- Variables ---
int rawSignal = 0;
int filterBuffer[FILTER_SIZE] = {0};
int filterIndex = 0;
long filterSum = 0;

int dcBuffer[DC_WINDOW] = {0};
int dcIndex = 0;
float dcValue = 0;
float acValue = 0;
float acdcRatio = 0;

bool peakDetected = false;
unsigned long lastBeatTime = 0;
unsigned long interval = 0;
int BPM = 0;

// HRV (RMSSD) variables
const int MAX_IBI = 50;
int ibiList[MAX_IBI];
int ibiIndex = 0;
bool ibiFull = false;
float RMSSD = 0;

// --------------------
// Moving average filter
// --------------------
int getFilteredPPG() {
  filterSum -= filterBuffer[filterIndex];
  filterBuffer[filterIndex] = analogRead(PPG_PIN);
  filterSum += filterBuffer[filterIndex];
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  return filterSum / FILTER_SIZE;
}

// --------------------
// Compute DC baseline
// --------------------
float getDC(int raw) {
  dcBuffer[dcIndex++] = raw;
  if(dcIndex >= DC_WINDOW) dcIndex = 0;
  long sum = 0;
  for(int i=0;i<DC_WINDOW;i++) sum += dcBuffer[i];
  return sum / (float)DC_WINDOW;
}

// --------------------
// Add IBI to list
// --------------------
void addIBI(int ibi) {
  if(ibi < 1200 && ibi > 300) { // only valid intervals
    ibiList[ibiIndex++] = ibi;
    if(ibiIndex >= MAX_IBI) {
      ibiIndex = 0;
      ibiFull = true;
    }
  }
}

// --------------------
// Compute RMSSD
// --------------------
float computeRMSSD() {
  int count = ibiFull ? MAX_IBI : ibiIndex;
  if(count < 2) return 0;

  float sumSq = 0;
  int validPairs = 0;
  for(int i=1;i<count;i++) {
    float diff = ibiList[i] - ibiList[i-1];
    sumSq += diff * diff;
    validPairs++;
  }
  return sqrt(sumSq / validPairs);
}

// --------------------
// Stress classification
// --------------------
String stressLevel(float rmssd, float acdc) {
  if(acdc > 2.0 && rmssd > 40) return "Relaxed";
  else if(acdc > 1.0 && acdc <= 2.0) return "Mild Stress";
  else return "High Stress / Anxiety";
}

// --------------------
// Setup
// --------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 PPG BPM + RMSSD + AC/DC Stress Monitor Ready!");
}

// --------------------
// Loop
// --------------------
void loop() {
  rawSignal = getFilteredPPG();
  dcValue = getDC(rawSignal);
  acValue = rawSignal - dcValue;

  // Calculate AC/DC ratio in percent
  acdcRatio = (dcValue>0) ? (acValue/dcValue)*100.0 : 0;

  // Peak detection
  if(acValue > MIN_PEAK_HEIGHT && (millis() - lastBeatTime) > MIN_BEAT_INTERVAL) {
    // Valid beat detected
    interval = millis() - lastBeatTime;
    lastBeatTime = millis();

    addIBI(interval);
    BPM = 60000 / interval;
    RMSSD = computeRMSSD();

    // Serial output
    Serial.print("BPM: "); Serial.print(BPM);
    Serial.print(" | RMSSD: "); Serial.print(RMSSD,1);
    Serial.print(" ms | AC/DC: "); Serial.print(acdcRatio,2);
    Serial.print("% | Stress: "); Serial.println(stressLevel(RMSSD, acdcRatio));
  }

  delay(10); // sample rate ~100Hz
}