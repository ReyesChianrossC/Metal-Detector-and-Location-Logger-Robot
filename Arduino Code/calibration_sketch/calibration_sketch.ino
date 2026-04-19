/*
  CALIBRATION SKETCH - Speed Measurement
  
  Purpose: Make robot go STRAIGHT at constant speed
           to measure actual speed for odometry calibration
  
  Instructions:
  1. Upload this to Arduino
  2. Mark 100cm distance on floor
  3. Place robot at start, power on
  4. Time how long to travel 100cm
  5. Calculate: Speed = 100 / time_in_seconds
  6. Update ROBOT_SPEED_CM_PER_SEC in ESP32Code.ino
  7. Re-upload your main sketch_jan8a.ino when done!
*/

// ===================== MOTOR PINS (TB6612FNG) =====================
const int PWMA = 3;
const int AIN1 = 5;
const int AIN2 = 6;

const int PWMB = 11;
const int BIN1 = 9;
const int BIN2 = 10;

const int STBY = 8;

// ===================== SPEED SETTING =====================
// Use the SAME speed as your main sketch!
// If robot curves RIGHT → Left motor is weaker → Increase LEFT_SPEED
// If robot curves LEFT  → Right motor is weaker → Increase RIGHT_SPEED
const int LEFT_MOTOR_SPEED = 55;   // Motor A - adjust if needed
const int RIGHT_MOTOR_SPEED = 53;  // Motor B - slightly lower to fix right curve

// Kick-start settings (same as main sketch)
const int KICK_SPEED = 120;
const unsigned long KICK_MS = 80;

// ===================== MOTOR FUNCTIONS =====================
void motorA(int speed, bool forward) {
  digitalWrite(AIN1, forward);
  digitalWrite(AIN2, !forward);
  analogWrite(PWMA, constrain(speed, 0, 255));
}

void motorB(int speed, bool forward) {
  digitalWrite(BIN1, forward);
  digitalWrite(BIN2, !forward);
  analogWrite(PWMB, constrain(speed, 0, 255));
}

void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}

void goStraight() {
  motorA(LEFT_MOTOR_SPEED, true);   // Left motor
  motorB(RIGHT_MOTOR_SPEED, true);  // Right motor
}

void kickStart() {
  motorA(KICK_SPEED, true);
  motorB(KICK_SPEED, true);
  delay(KICK_MS);
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  
  // Motor pins
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  
  // Enable motor driver
  digitalWrite(STBY, HIGH);
  
  Serial.println("=====================================");
  Serial.println("    CALIBRATION MODE - GO STRAIGHT   ");
  Serial.println("=====================================");
  Serial.println("");
  Serial.println("1. Mark 100cm on floor");
  Serial.println("2. Place robot at START");
  Serial.println("3. Start your stopwatch!");
  Serial.println("4. Time until robot reaches END");
  Serial.println("5. Calculate: Speed = 100 / seconds");
  Serial.println("");
  Serial.println("Robot starting in 3 seconds...");
  delay(3000);
  
  Serial.println("GO! Start your timer NOW!");
  
  // Kick-start then go straight
  kickStart();
}

// ===================== LOOP =====================
void loop() {
  // Just go straight forever
  goStraight();
  delay(10);
}

