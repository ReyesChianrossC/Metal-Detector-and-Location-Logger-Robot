/*
  TUNNEL SWEEPING METAL DETECTOR ROVER
  - Zigzag pattern to scan entire tunnel width
  - Ultrasonic sensor prevents wall collisions
  - Stops immediately when metal detected
*/

// ===================== MOTOR PINS (TB6612FNG) =====================
const int PWMA = 3;
const int AIN1 = 5;
const int AIN2 = 6;

const int PWMB = 11;
const int BIN1 = 9;
const int BIN2 = 10;

const int STBY = 8;

// ===================== ULTRASONIC SENSOR =====================
const int TRIG_PIN = A1;
const int ECHO_PIN = A2;
const int WALL_TOO_CLOSE_CM = 25;   // Detect wall MUCH earlier (25cm!) - single forward sensor
const unsigned long REVERSE_MS = 300;  // How long to reverse before turning

// ===================== METAL DETECTOR =====================
const int METAL_ANALOG_PIN = A0;
const int METAL_FLAG_OUT_PIN = 12;  // Signal to ESP32-CAM (GPIO14)

// ===================== FIND ME BUZZER =====================
const int BUZZER_PIN = 7;           // Buzzer for "Find Me" feature
const int FIND_ME_INPUT_PIN = 4;    // Input from ESP32 (GPIO13) to trigger buzzer
bool findMeActive = false;
unsigned long lastBeepTime = 0;

// ===================== SPEED SETTINGS =====================
// INCREASED for heavier robot!
const int SPEED_FAST = 85;       // Faster wheel (for turning) - increased from 65
const int SPEED_SLOW = 55;       // Slower wheel (for turning) - increased from 35
const int SPEED_STRAIGHT = 70;   // Both wheels (brief straight) - increased from 55

// WALL AVOIDANCE - REVERSE then TURN HARD!
const int REVERSE_SPEED = 80;    // Speed when backing up from wall
const int WALL_TURN_FAST = 100;  // Very fast turn away from wall
const int WALL_TURN_SLOW = 20;   // Slow motor (sharp turn!)

// Kick-start to overcome friction
const int KICK_SPEED = 150;      // Increased kick-start
const unsigned long KICK_MS = 100;

// ===================== SWEEP PATTERN TIMING =====================
const unsigned long SWEEP_LEFT_MS = 800;   // Time sweeping left
const unsigned long SWEEP_RIGHT_MS = 800;  // Time sweeping right
const unsigned long SWEEP_STRAIGHT_MS = 200; // Brief straight between sweeps

// ===================== METAL SENSITIVITY =====================
const int METAL_THRESHOLD = 350;  // Lower = more sensitive
const unsigned long METAL_CHECK_MS = 2;

// ===================== STATE =====================
bool metalDetected = false;
bool wasMetalDetected = false;
unsigned long metalClearedTime = 0;
const unsigned long RESUME_DELAY_MS = 500;

// Sweep state
enum SweepState { SWEEP_LEFT, SWEEP_STRAIGHT_1, SWEEP_RIGHT, SWEEP_STRAIGHT_2 };
SweepState currentSweep = SWEEP_LEFT;
unsigned long sweepStartTime = 0;

// Wall avoidance state (for single forward sensor)
enum WallAvoidState { NO_WALL, REVERSING, TURNING_AWAY };
WallAvoidState wallState = NO_WALL;
unsigned long wallActionStartTime = 0;
bool turnDirection = true;  // true = turn right, false = turn left (alternate)

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

// Sweep left while moving forward
void sweepLeft() {
  motorA(SPEED_SLOW, true);
  motorB(SPEED_FAST, true);
}

// Sweep right while moving forward
void sweepRight() {
  motorA(SPEED_FAST, true);
  motorB(SPEED_SLOW, true);
}

// Reverse backward
void reverse() {
  motorA(REVERSE_SPEED, false);  // false = backward
  motorB(REVERSE_SPEED, false);
}

// STRONG wall avoidance turns - sharp turns!
void wallAvoidLeft() {
  motorA(WALL_TURN_SLOW, true);   // Very slow (sharp turn)
  motorB(WALL_TURN_FAST, true);   // Very fast (sharp turn)
}

void wallAvoidRight() {
  motorA(WALL_TURN_FAST, true);   // Very fast (sharp turn)
  motorB(WALL_TURN_SLOW, true);   // Very slow (sharp turn)
}

// Go straight
void goStraight() {
  motorA(SPEED_STRAIGHT, true);
  motorB(SPEED_STRAIGHT, true);
}

void kickStart() {
  motorA(KICK_SPEED, true);
  motorB(KICK_SPEED, true);
  delay(KICK_MS);
}

// ===================== ULTRASONIC SENSOR =====================
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 15000UL);  // 15ms timeout
  if (duration == 0) return 999;  // No echo = far away
  return duration * 0.0343 / 2;
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
  
  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Metal detector output to ESP32
  pinMode(METAL_FLAG_OUT_PIN, OUTPUT);
  digitalWrite(METAL_FLAG_OUT_PIN, LOW);
  
  // Find Me buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Find Me input from ESP32
  pinMode(FIND_ME_INPUT_PIN, INPUT);
  
  // Enable motor driver
  digitalWrite(STBY, HIGH);
  
  // Initial kick-start
  kickStart();
  
  // Start sweeping
  currentSweep = SWEEP_LEFT;
  sweepStartTime = millis();
  
  Serial.println("=========================================");
  Serial.println("TUNNEL SWEEPING ROVER WITH WALL DETECTION");
  Serial.println("Ultrasonic prevents wall collisions!");
  Serial.println("=========================================");
}

// ===================== SWEEP PATTERN =====================
void updateSweepPattern() {
  unsigned long now = millis();
  unsigned long elapsed = now - sweepStartTime;
  
  // Check wall distance (single forward sensor)
  long wallDist = readDistanceCm();
  bool wallTooClose = (wallDist < WALL_TOO_CLOSE_CM);
  
  // Debug: Print distance every 500ms
  static unsigned long lastDistPrint = 0;
  if (now - lastDistPrint > 500) {
    lastDistPrint = now;
    Serial.print("[ULTRASONIC] Distance: ");
    Serial.print(wallDist);
    Serial.print(" cm | State: ");
    switch(wallState) {
      case NO_WALL: Serial.print("NORMAL"); break;
      case REVERSING: Serial.print("REVERSING"); break;
      case TURNING_AWAY: Serial.print("TURNING"); break;
    }
    Serial.println();
  }
  
  // ========== WALL AVOIDANCE STATE MACHINE (Single Forward Sensor) ==========
  if (wallState == NO_WALL && wallTooClose) {
    // Wall detected! Start reversing
    wallState = REVERSING;
    wallActionStartTime = now;
    turnDirection = !turnDirection;  // Alternate turn direction
    Serial.print("[WALL] Detected at ");
    Serial.print(wallDist);
    Serial.println(" cm - REVERSING!");
    reverse();
    return;  // Exit - don't do normal sweep pattern
  }
  
  if (wallState == REVERSING) {
    // Keep reversing for REVERSE_MS
    reverse();
    if (now - wallActionStartTime >= REVERSE_MS) {
      // Done reversing, now turn away!
      wallState = TURNING_AWAY;
      wallActionStartTime = now;
      kickStart();  // Power boost!
      if (turnDirection) {
        wallAvoidRight();  // Turn right
        Serial.println("[WALL] Reversing done - TURNING RIGHT!");
      } else {
        wallAvoidLeft();   // Turn left
        Serial.println("[WALL] Reversing done - TURNING LEFT!");
      }
    }
    return;  // Exit - don't do normal sweep pattern
  }
  
  if (wallState == TURNING_AWAY) {
    // Keep turning for 600ms
    if (turnDirection) {
      wallAvoidRight();
    } else {
      wallAvoidLeft();
    }
    
    // Check if wall is clear now
    if (!wallTooClose && (now - wallActionStartTime >= 600)) {
      // Wall cleared! Resume normal pattern
      wallState = NO_WALL;
      sweepStartTime = now;
      Serial.println("[WALL] Clear! Resuming normal pattern.");
    }
    return;  // Exit - don't do normal sweep pattern
  }
  
  // ========== NORMAL SWEEP PATTERN (No Wall) ==========
  switch (currentSweep) {
    case SWEEP_LEFT:
      sweepLeft();
      if (elapsed >= SWEEP_LEFT_MS) {
        currentSweep = SWEEP_STRAIGHT_1;
        sweepStartTime = now;
      }
      break;
      
    case SWEEP_STRAIGHT_1:
      goStraight();
      if (elapsed >= SWEEP_STRAIGHT_MS) {
        currentSweep = SWEEP_RIGHT;
        sweepStartTime = now;
      }
      break;
      
    case SWEEP_RIGHT:
      sweepRight();
      if (elapsed >= SWEEP_RIGHT_MS) {
        currentSweep = SWEEP_STRAIGHT_2;
        sweepStartTime = now;
      }
      break;
      
    case SWEEP_STRAIGHT_2:
      goStraight();
      if (elapsed >= SWEEP_STRAIGHT_MS) {
        currentSweep = SWEEP_LEFT;
        sweepStartTime = now;
      }
      break;
  }
}

// ===================== MAIN LOOP =====================
void loop() {
  static unsigned long lastMetalCheck = 0;
  unsigned long now = millis();
  
  
  // ========== FIND ME BUZZER ==========
  // Check if ESP32 is requesting "Find Me" beep
  if (digitalRead(FIND_ME_INPUT_PIN) == HIGH) {
    // Beep pattern: 200ms on, 200ms off
    if (now - lastBeepTime >= 200) {
      lastBeepTime = now;
      findMeActive = !findMeActive;
      digitalWrite(BUZZER_PIN, findMeActive ? HIGH : LOW);
    }
  } else {
    // Turn off buzzer when Find Me is not active
    if (findMeActive) {
      digitalWrite(BUZZER_PIN, LOW);
      findMeActive = false;
    }
  }
  
  // ========== CHECK METAL DETECTOR ==========
  if (now - lastMetalCheck >= METAL_CHECK_MS) {
    lastMetalCheck = now;
    
    int metalReading = analogRead(METAL_ANALOG_PIN);
    
    if (metalReading >= METAL_THRESHOLD) {
      // METAL DETECTED!
      if (!metalDetected) {
        Serial.print("[METAL FOUND!] Reading: ");
        Serial.println(metalReading);
      }
      metalDetected = true;
      wasMetalDetected = true;
      digitalWrite(METAL_FLAG_OUT_PIN, HIGH);
      
    } else {
      // No metal
      if (metalDetected) {
        metalClearedTime = now;
        Serial.println("[CLEAR] Metal no longer detected");
      }
      metalDetected = false;
      digitalWrite(METAL_FLAG_OUT_PIN, LOW);
    }
  }
  
  // ========== MOTOR CONTROL ==========
  if (metalDetected) {
    // STOP when metal detected
    stopMotors();
    
  } else if (wasMetalDetected) {
    // Wait before resuming
    if (now - metalClearedTime >= RESUME_DELAY_MS) {
      Serial.println("[RESUME] Continuing sweep pattern");
      wasMetalDetected = false;
      kickStart();
      sweepStartTime = now;
    } else {
      stopMotors();
    }
    
  } else {
    // Normal operation - sweep pattern with wall avoidance
    updateSweepPattern();
  }
  
  delay(5);
}
