// ===============================
// XD-58C Signal Visualizer + HRV
// ===============================

const int PULSE_PIN = 36;

int sensorValue = 0;
int prevValue = 0;
int baseline = 2000;
int minSig = 4095;
int maxSig = 0;

// Beat detection
bool lookingForBeat = false;
unsigned long lastBeat = 0;

// Intervals storage
unsigned long intervals[15];
int idx = 0;
int count = 0;

// HRV smoothing
float smoothRMSSD = 0;
float smoothSDNN = 0;

int sampleCount = 0;

// ===============================
void setup() {
  Serial.begin(115200);
  pinMode(PULSE_PIN, INPUT);
  delay(1000);
  
  Serial.println("\n=== XD-58C HRV Monitor ===");
  Serial.println("Calibrating for 3 seconds...");
  
  // Calibrate
  long sum = 0;
  for(int i = 0; i < 300; i++) {
    int r = analogRead(PULSE_PIN);
    sum += r;
    if(r < minSig) minSig = r;
    if(r > maxSig) maxSig = r;
    delay(10);
  }
  baseline = sum / 300;
  
  Serial.print("✓ Baseline: ");
  Serial.println(baseline);
  Serial.print("✓ Range: ");
  Serial.print(minSig);
  Serial.print(" - ");
  Serial.println(maxSig);
  Serial.print("✓ Swing: ");
  Serial.println(maxSig - minSig);
  
  if(maxSig - minSig < 50) {
    Serial.println("\n⚠️  Signal too weak - check finger placement!");
  }
  
  Serial.println("\n--- Live Signal (updates every 0.5s) ---");
  delay(1000);
}

// ===============================
void loop() {
  prevValue = sensorValue;
  sensorValue = analogRead(PULSE_PIN);
  
  // Update baseline slowly
  baseline = (baseline * 98 + sensorValue * 2) / 100;
  
  // Track min/max
  if(sensorValue < minSig) minSig = sensorValue;
  if(sensorValue > maxSig) maxSig = sensorValue;
  
  // Print signal graph every 50 samples (0.5 sec)
  sampleCount++;
  if(sampleCount >= 50) {
    sampleCount = 0;
    
    // Visual bar graph
    Serial.print("Sig:");
    int bars = map(sensorValue, 0, 4095, 0, 50);
    for(int i = 0; i < bars; i++) Serial.print("█");
    Serial.print(" ");
    Serial.print(sensorValue);
    Serial.print(" | Base:");
    Serial.print(baseline);
    Serial.print(" | Diff:");
    Serial.print(sensorValue - baseline);
    Serial.print(" | Beats:");
    Serial.println(count);
    
    // Reset range
    minSig = sensorValue;
    maxSig = sensorValue;
  }
  
  // BEAT DETECTION
  // XD-58C: look for signal crossing BELOW baseline
  
  if(sensorValue > baseline + 100) {
    lookingForBeat = true;
  }
  
  if(lookingForBeat && sensorValue < baseline - 100) {
    unsigned long now = millis();
    unsigned long ibi = now - lastBeat;
    
    if(ibi > 400 && ibi < 1500 && lastBeat > 0) {
      // Valid beat!
      intervals[idx] = ibi;
      idx = (idx + 1) % 15;
      if(count < 15) count++;
      
      if(count >= 8) {
        // Calculate BPM
        unsigned long sumIBI = 0;
        for(int i = 0; i < count; i++) sumIBI += intervals[i];
        int bpm = 60000 / (sumIBI / count);
        
        // Calculate SDNN
        float meanIBI = sumIBI / (float)count;
        float varSum = 0;
        for(int i = 0; i < count; i++) {
          float d = intervals[i] - meanIBI;
          varSum += d * d;
        }
        float sdnn = sqrt(varSum / count);
        
        // Calculate RMSSD
        float diffSum = 0;
        for(int i = 1; i < count; i++) {
          float d = (float)intervals[i] - (float)intervals[i-1];
          diffSum += d * d;
        }
        float rmssd = sqrt(diffSum / (count - 1));
        
        // Smooth
        if(smoothRMSSD == 0) {
          smoothRMSSD = rmssd;
          smoothSDNN = sdnn;
        } else {
          smoothRMSSD = smoothRMSSD * 0.8 + rmssd * 0.2;
          smoothSDNN = smoothSDNN * 0.8 + sdnn * 0.2;
        }
        
        Serial.print(">>> BPM:");
        Serial.print(bpm);
        Serial.print(" SDNN:");
        Serial.print(smoothSDNN, 1);
        Serial.print(" RMSSD:");
        Serial.print(smoothRMSSD, 1);
        Serial.println(" <<<");
      }
    }
    
    lastBeat = now;
    lookingForBeat = false;
  }
  
  delay(10);
}