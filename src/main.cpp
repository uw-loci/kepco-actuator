#include <Arduino.h>
#include <Servo.h>

constexpr uint8_t SERVO_PIN = 10; // Arduino Uno D10
constexpr uint8_t TRIGGER_PIN = 2; // Arduino Uno D2
constexpr int SERVO_MIN_PULSE_US = 500;
constexpr int SERVO_MAX_PULSE_US = 2500;
constexpr float SERVO_RANGE_DEGREES = 270.0f;
constexpr unsigned long TRIGGER_DEBOUNCE_MS = 5;

Servo actuatorServo;
bool lastTriggerReading = HIGH;
bool stableTriggerState = HIGH;
unsigned long lastTriggerChangeMs = 0;

void writeServoDegrees(float degrees);
void updateTriggerInput();
void handleTriggerShortedToGround();

void setup() {
  actuatorServo.attach(SERVO_PIN, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  writeServoDegrees(SERVO_RANGE_DEGREES / 2.0f);  // TODO: start at middle?
}

void loop() {
  updateTriggerInput();
}

void writeServoDegrees(float degrees) {
  const float constrainedDegrees = constrain(degrees, 0.0f, SERVO_RANGE_DEGREES);
  const int pulseWidthUs = SERVO_MIN_PULSE_US +
                           ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) *
                            constrainedDegrees / SERVO_RANGE_DEGREES);

  actuatorServo.writeMicroseconds(pulseWidthUs);
}

void updateTriggerInput() {
  const bool triggerReading = digitalRead(TRIGGER_PIN);

  if (triggerReading != lastTriggerReading) {
    lastTriggerChangeMs = millis();
    lastTriggerReading = triggerReading;
  }

  if ((millis() - lastTriggerChangeMs) >= TRIGGER_DEBOUNCE_MS &&
      triggerReading != stableTriggerState) {
    stableTriggerState = triggerReading;

    if (stableTriggerState == LOW) {
      handleTriggerShortedToGround();
    }
  }
}

void handleTriggerShortedToGround() {
  // Activate the servo to move to a specific position when the activate button is pressed (shorted to ground).
  writeServoDegrees(0.0f); // move to 0 degrees - TODO: change degrees number
  delay(1000); // hold for 1 second - TODO: change duration
  writeServoDegrees(0.0f); // move to 0 degrees - TODO: change degrees number
  delay(1000); // hold for 1 second - TODO: change duration
}
