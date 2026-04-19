/*
  Robot: TB6612FNG + 2 motors + MDS-60 metal detector (analog) + ESP32-CAM trigger

  TB6612 pins (as previously working):
    AIN1 D5
    AIN2 D6
    PWMA D3
    BIN1 D9
    BIN2 D10
    PWMB D11
    STBY D8

  Metal detector:
    metal analog signal -> A0
    threshold tuned from your readings:
      no metal ~200-240
      metal ~990-1018
    so threshold = 500 is safe

  ESP32-CAM trigger:
    Arduino D7 -> ESP32-CAM GPIO13
*/

const int AIN1 = 5;
const int AIN2 = 6;
const int PWMA = 3;

const int BIN1 = 9;
const int BIN2 = 10;
const int PWMB = 11;

const int STBY = 8;

const int METAL_A0 = A0;
const int CAM_TRIG = 7;

const int METAL_THRESHOLD = 500;   // based on your values
const int DRIVE_SPEED = 180;       // 0-255
const int TURN_SPEED  = 170;

unsigned long nextDecisionMs = 0;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);

  pinMode(STBY, OUTPUT);
  pinMode(CAM_TRIG, OUTPUT);

  digitalWrite(STBY, HIGH);        // wake TB6612
  digitalWrite(CAM_TRIG, LOW);     // default

  randomSeed(analogRead(A1));      // randomness for wandering
}

void loop() {
  int metal = analogRead(METAL_A0);

  // ---- METAL DETECTED BEHAVIOR ----
  if (metal > METAL_THRESHOLD) {
    stopMotors();
    digitalWrite(CAM_TRIG, HIGH);      // tell ESP32-CAM "metal found"

    // Optional: hold position a bit so camera can react
    delay(2000);

    // Optional: move away a little, then stop again
    driveBackward(160);
    delay(400);
    stopMotors();
    delay(300);

    return; // keep checking metal continuously
  } else {
    digitalWrite(CAM_TRIG, LOW);
  }

  // ---- WANDER (NO ULTRASONIC) ----
  // Forward most of the time; occasionally turn.
  unsigned long now = millis();
  if (now >= nextDecisionMs) {
    int r = random(0, 100);

    if (r < 70) {
      // go forward
      driveForward(DRIVE_SPEED);
      nextDecisionMs = now + random(800, 1800);
    } else if (r < 85) {
      // turn left
      turnLeft(TURN_SPEED);
      nextDecisionMs = now + random(250, 600);
    } else {
      // turn right
      turnRight(TURN_SPEED);
      nextDecisionMs = now + random(250, 600);
    }
  }

  // small delay to keep loop stable
  delay(10);
}

// ---------------- MOTOR HELPERS ----------------

void driveForward(int spd) {
  // Motor A forward
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, spd);

  // Motor B forward
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, spd);
}

void driveBackward(int spd) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, spd);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, spd);
}

void turnLeft(int spd) {
  // left motor backward, right motor forward
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, spd);

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, spd);
}

void turnRight(int spd) {
  // left motor forward, right motor backward
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, spd);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, spd);
}

void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}
