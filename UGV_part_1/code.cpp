#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#include <DabbleESP32.h>

// ESP32 GPIO mapping based on schematic
const int IN1_PIN = 32; // Motor A
const int IN2_PIN = 33; // Motor A
const int IN3_PIN = 25; // Motor B
const int IN4_PIN = 26; // Motor B

// PWM Speed (0 to 255)
const int CRUISE_SPEED = 200; 

void driveMotorA(int speed, bool forward) {
  if (speed == 0) {
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, 0);
  } else if (forward) {
    analogWrite(IN1_PIN, speed);
    analogWrite(IN2_PIN, 0);
  } else {
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, speed);
  }
}

void driveMotorB(int speed, bool forward) {
  if (speed == 0) {
    analogWrite(IN3_PIN, 0);
    analogWrite(IN4_PIN, 0);
  } else if (forward) {
    analogWrite(IN3_PIN, speed);
    analogWrite(IN4_PIN, 0);
  } else {
    analogWrite(IN3_PIN, 0);
    analogWrite(IN4_PIN, speed);
  }
}

void stopUGV() {
  driveMotorA(0, true);
  driveMotorB(0, true);
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  stopUGV();

  // Initialize Bluetooth with custom device name
  Dabble.begin("UGV_ESP32");
  Serial.println("UGV Firmware Started. Connect via Dabble app.");
}

void loop() {
  Dabble.processInput();

  if (GamePad.isUpPressed()) {
    // Forward
    driveMotorA(CRUISE_SPEED, true);
    driveMotorB(CRUISE_SPEED, true);
  } else if (GamePad.isDownPressed()) {
    // Reverse
    driveMotorA(CRUISE_SPEED, false);
    driveMotorB(CRUISE_SPEED, false);
  } else if (GamePad.isLeftPressed()) {
    // Pivot Left
    driveMotorA(CRUISE_SPEED, false);
    driveMotorB(CRUISE_SPEED, true);
  } else if (GamePad.isRightPressed()) {
    // Pivot Right
    driveMotorA(CRUISE_SPEED, true);
    driveMotorB(CRUISE_SPEED, false);
  } else {
    stopUGV();
  }
}
