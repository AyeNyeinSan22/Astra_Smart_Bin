#include <Servo.h>

// --- BIN 1: PLASTIC ---
const int pTrig = 2;
const int pEcho = 4;
const int pServoPin = 3;

// --- BIN 2: PAPER ---
const int sTrig = 7;
const int sEcho = 9;
const int sServoPin = 5;

// --- CONFIGURATION ---
const int binDepth = 30;
const int fullThreshold = 5;

// Servo 1: Plastic (180 Degree)
const int pOpenAngle = 50;
const int pCloseAngle = 0; 

// Servo 2: Paper (360 Degree)
const int sSpinSpeedOpen = 75;   // CCW to open
const int sSpinSpeedClose = 75;    // CW to close
const int sStopSignal = 90;  
const int sOpenTime = 800;   
const int sCloseTime = 800;  

const int lidOpenTime = 4000; 

Servo servoPlastic;
Servo servoPaper;

void setup() {
  Serial.begin(9600);

  pinMode(pTrig, OUTPUT); pinMode(pEcho, INPUT);
  pinMode(sTrig, OUTPUT); pinMode(sEcho, INPUT);

  // We will attach/detach dynamically for safety
  // pinMode(pServoPin, OUTPUT);
  // pinMode(sServoPin, OUTPUT);

  // Initial states
  servoPlastic.write(pCloseAngle);
  servoPaper.write(sStopSignal); // Make sure 360 servo is stopped

  Serial.println("================================");
  Serial.println("   SmartBin System Starting..  ");
  Serial.println("================================");

  delay(500);
}

void loop() {
  // 1. LISTEN: Receive commands from AI
  if (Serial.available() > 0) {
    char aiSignal = Serial.read();

    if (aiSignal == 'P') {
      Serial.println(">> AI Signal: PLASTIC (180 Servo)");
      openPlasticLid();
    }
    else if (aiSignal == 'S') {
      Serial.println(">> AI Signal: PAPER (360 Servo)");
      openPaperLid();
    }
  }

  // 2. MONITOR: Report fill levels every 3 seconds
  static unsigned long lastReportingTime = 0;
  if (millis() - lastReportingTime > 3000) {
    int plasticDist = getDistance(pTrig, pEcho);
    int paperDist   = getDistance(sTrig, sEcho);
    
    int plasticPercent = calculatePercent(plasticDist);
    int paperPercent   = calculatePercent(paperDist);

    // --- SERIAL MONITOR DISPLAY ---
    Serial.println("--------------------------------");
    Serial.print("Plastic Bin : ");
    Serial.print(plasticPercent);
    Serial.print("% ("); Serial.print(plasticDist); Serial.print("cm) ");
    Serial.println(getBar(plasticPercent));

    Serial.print("Paper Bin   : ");
    Serial.print(paperPercent);
    Serial.print("% ("); Serial.print(paperDist); Serial.print("cm) ");
    Serial.println(getBar(paperPercent));

    // Warning messages
    if (plasticPercent >= 90) Serial.println("!! WARNING: Plastic bin is FULL !!");
    if (paperPercent >= 90)   Serial.println("!! WARNING: Paper bin is FULL !!");

    Serial.println("--------------------------------");

    // Data packet for Python/database
    Serial.print("DATA|");
    Serial.print(plasticPercent);
    Serial.print("|");
    Serial.println(paperPercent);

    lastReportingTime = millis();
  }
}

// --- Visual bar graph in Serial Monitor ---
String getBar(int percent) {
  String bar = "[";
  int filled = percent / 10; // 10 blocks max
  for (int i = 0; i < 10; i++) {
    if (i < filled) bar += "#";
    else bar += "-";
  }
  bar += "]";
  return bar;
}

// --- Plastic Bin (Standard 180 Servo) ---
void openPlasticLid() {
  Serial.println(">>> SAFETY: Attaching Plastic Servo...");
  servoPlastic.attach(pServoPin);
  delay(100); // Wait for power to stabilize
  
  Serial.println(">>> [PLASTIC] Moving to 30 degrees...");
  servoPlastic.write(pOpenAngle);
  delay(lidOpenTime);
  
  Serial.println(">>> [PLASTIC] Returning to 0 degrees...");
  servoPlastic.write(pCloseAngle);
  delay(500); // Allow time to reach 0
  
  Serial.println(">>> SAFETY: Detaching Plastic Servo...");
  servoPlastic.detach();
}

// --- Paper Bin (Continuous 360 Servo) ---
void openPaperLid() {
  Serial.println(">>> SAFETY: Attaching Paper Servo...");
  servoPaper.attach(sServoPin);
  delay(100);
  
  Serial.println(">>> [PAPER] Spinning COUNTER-CLOCKWISE to open...");
  servoPaper.write(sSpinSpeedOpen); 
  delay(sOpenTime);
  
  servoPaper.write(sStopSignal); // Stop and wait
  Serial.println(">>> [PAPER] Lid Open. Waiting...");
  delay(lidOpenTime);
  
  Serial.println(">>> [PAPER] Spinning CLOCKWISE to close...");
  servoPaper.write(sSpinSpeedClose); 
  delay(sCloseTime);
  
  servoPaper.write(sStopSignal);
  delay(200);
  
  Serial.println(">>> SAFETY: Detaching Paper Servo...");
  servoPaper.detach();
}


// --- Calculate distance in cm ---
int getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 25000);
  if (duration == 0) return 999; // 999 means no reading/timeout

  return duration * 0.034 / 2;
}

// --- Convert distance to percent ---
int calculatePercent(int distance) {
  if (distance > binDepth) return 0;
  int percent = map(distance, binDepth, fullThreshold, 0, 100);
  return constrain(percent, 0, 100);
}