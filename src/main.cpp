#include <Arduino.h>
#include <Servo.h>
#include <avr/wdt.h>

// Hardware pins and servo calibration values.
constexpr uint8_t SERVO_PIN = 10; // Arduino Uno D10
constexpr uint8_t TRIGGER_PIN = 2; // Arduino Uno D2
constexpr int SERVO_MIN_PULSE_US = 500;
constexpr int SERVO_MAX_PULSE_US = 2500;
constexpr float SERVO_RANGE_DEGREES = 270.0f;
constexpr unsigned long TRIGGER_DEBOUNCE_MS = 5;

// Runtime state for the actuator and debounced trigger input.
Servo actuatorServo;
bool lastTriggerReading = HIGH;
bool stableTriggerState = HIGH;
unsigned long lastTriggerChangeMs = 0;

// Capture reset cause and stop any inherited watchdog before normal startup runs.
// This follows the standard avr-libc early-startup watchdog pattern.
uint8_t resetCauseMirror __attribute__((section(".noinit")));
void watchdog_early_init(void) __attribute__((naked)) __attribute__((section(".init3"))) __attribute__((used));

void watchdog_early_init(void) {
  resetCauseMirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

void writeServoDegrees(float degrees);
void updateTriggerInput();
void runServoSequence();

void setup() {
  // Initialize the servo and trigger input before enabling the watchdog.
  actuatorServo.attach(SERVO_PIN, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  writeServoDegrees(SERVO_RANGE_DEGREES / 2.0f);  // TODO: start at middle?

  // Reset the board if firmware stops servicing the watchdog for 4 seconds.
  wdt_enable(WDTO_4S);
}

void loop() {
  // Poll the trigger each pass, then confirm the main loop is still healthy.
  updateTriggerInput();
  wdt_reset();
}

void writeServoDegrees(float degrees) {
  // Convert logical actuator degrees into the calibrated servo pulse width.
  const float constrainedDegrees = constrain(degrees, 0.0f, SERVO_RANGE_DEGREES);
  const int pulseWidthUs = SERVO_MIN_PULSE_US +
                           ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) *
                            constrainedDegrees / SERVO_RANGE_DEGREES);

  actuatorServo.writeMicroseconds(pulseWidthUs);
}

void updateTriggerInput() {
  // Debounce the pullup input so only stable low transitions fire the actuator.
  const bool triggerReading = digitalRead(TRIGGER_PIN);

  if (triggerReading != lastTriggerReading) {
    lastTriggerChangeMs = millis();
    lastTriggerReading = triggerReading;
  }

  if ((millis() - lastTriggerChangeMs) >= TRIGGER_DEBOUNCE_MS && triggerReading != stableTriggerState) {
    stableTriggerState = triggerReading;

    if (stableTriggerState == LOW) {
      runServoSequence();
    }
  }
}

void runServoSequence() {
  // Run the actuator sequence: up, wait, down, wait
  writeServoDegrees(0.0f); // move to 0 degrees - TODO: change degrees number
  delay(1000); // hold for 1 second - TODO: change duration
  writeServoDegrees(0.0f); // move to 0 degrees - TODO: change degrees number
  delay(1000); // hold for 1 second - TODO: change duration
}
