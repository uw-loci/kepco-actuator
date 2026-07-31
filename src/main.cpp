#include <Arduino.h>
#include <Servo.h>
#include <avr/wdt.h>

// Hardware pins.
constexpr uint8_t KEPCO_SERVO_PIN = 10;     // Arduino Uno D10
constexpr uint8_t GLASSMAN_SERVO_PIN = 9;   // Arduino Uno D9
constexpr uint8_t TRIGGER_BUTTON_PIN = 2;   // Arduino Uno D2 to button, using INPUT_PULLUP

// Kepco servo calibration and sequence parameters.
constexpr int KEPCO_SERVO_MIN_PULSE_US = 500;
constexpr int KEPCO_SERVO_MAX_PULSE_US = 2500;
constexpr float KEPCO_SERVO_RANGE_DEGREES = 270.0f;
constexpr float KEPCO_SERVO_REST_DEGREES = 0.0f;
constexpr float KEPCO_SERVO_ACTIVE_DEGREES = 100.0f;
constexpr unsigned long KEPCO_SERVO_ACTIVE_HOLD_MS = 1000;
constexpr unsigned long KEPCO_SERVO_REST_HOLD_MS = 1000;

// Glassman Tower Pro SG90 calibration and sequence parameters.
// The conservative initial pulse limits and all motion values should be
// adjusted after testing with the final mechanical installation.
constexpr int GLASSMAN_SERVO_MIN_PULSE_US = 1000;
constexpr int GLASSMAN_SERVO_MAX_PULSE_US = 2000;
constexpr float GLASSMAN_SERVO_RANGE_DEGREES = 150.0f;
constexpr float GLASSMAN_SERVO_REST_DEGREES = 90.0f;
constexpr float GLASSMAN_SERVO_ACTIVE_DEGREES = 150.0f;
constexpr unsigned long GLASSMAN_SERVO_ACTIVE_HOLD_MS = 1000;
constexpr unsigned long GLASSMAN_SERVO_REST_HOLD_MS = 1000;

// A stable release selects the servo from the debounced press duration.
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;
constexpr unsigned long GLASSMAN_PRESS_THRESHOLD_MS = 5000;

enum class ServoSequencePhase : uint8_t {
  IDLE,
  HOLDING_ACTIVE,
  HOLDING_REST
};

struct ServoSequenceState {
  ServoSequencePhase phase;
  unsigned long phaseStartedMs;
};

Servo kepcoServo;
Servo glassmanServo;
ServoSequenceState kepcoSequence = {ServoSequencePhase::IDLE, 0};
ServoSequenceState glassmanSequence = {ServoSequencePhase::IDLE, 0};

// Debounced button state. The pullup makes LOW pressed and HIGH released.
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
bool buttonPressInProgress = false;
unsigned long lastButtonReadingChangeMs = 0;
unsigned long buttonPressedMs = 0;

// Capture reset cause and stop any inherited watchdog before normal startup runs.
// This follows the standard avr-libc early-startup watchdog pattern.
uint8_t resetCauseMirror __attribute__((section(".noinit")));
void watchdog_early_init(void) __attribute__((naked)) __attribute__((section(".init3"))) __attribute__((used));

void watchdog_early_init(void) {
  resetCauseMirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

void writeServoDegrees(Servo &servo, float degrees, int minPulseUs,
                       int maxPulseUs, float rangeDegrees);
void updateButton(unsigned long nowMs);
bool isServoSequenceRunning();
void startKepcoSequence(unsigned long nowMs);
void startGlassmanSequence(unsigned long nowMs);
void updateServoSequence(Servo &servo, ServoSequenceState &sequence,
                         unsigned long nowMs, float restDegrees,
                         unsigned long activeHoldMs,
                         unsigned long restHoldMs, int minPulseUs,
                         int maxPulseUs, float rangeDegrees);

void setup() {
  // Servos need an appropriately sized external supply with its ground tied
  // to Arduino ground. Do not power both servos directly from the Uno 5 V pin.
  kepcoServo.attach(KEPCO_SERVO_PIN, KEPCO_SERVO_MIN_PULSE_US,
                    KEPCO_SERVO_MAX_PULSE_US);
  glassmanServo.attach(GLASSMAN_SERVO_PIN, GLASSMAN_SERVO_MIN_PULSE_US,
                       GLASSMAN_SERVO_MAX_PULSE_US);
  pinMode(TRIGGER_BUTTON_PIN, INPUT_PULLUP);

  writeServoDegrees(kepcoServo, KEPCO_SERVO_REST_DEGREES,
                    KEPCO_SERVO_MIN_PULSE_US, KEPCO_SERVO_MAX_PULSE_US,
                    KEPCO_SERVO_RANGE_DEGREES);
  writeServoDegrees(glassmanServo, GLASSMAN_SERVO_REST_DEGREES,
                    GLASSMAN_SERVO_MIN_PULSE_US,
                    GLASSMAN_SERVO_MAX_PULSE_US,
                    GLASSMAN_SERVO_RANGE_DEGREES);

  // Reset the board if firmware stops servicing the watchdog for 4 seconds.
  wdt_enable(WDTO_4S);
}

void loop() {
  const unsigned long nowMs = millis();

  updateButton(nowMs);
  updateServoSequence(kepcoServo, kepcoSequence, nowMs,
                      KEPCO_SERVO_REST_DEGREES,
                      KEPCO_SERVO_ACTIVE_HOLD_MS,
                      KEPCO_SERVO_REST_HOLD_MS,
                      KEPCO_SERVO_MIN_PULSE_US,
                      KEPCO_SERVO_MAX_PULSE_US,
                      KEPCO_SERVO_RANGE_DEGREES);
  updateServoSequence(glassmanServo, glassmanSequence, nowMs,
                      GLASSMAN_SERVO_REST_DEGREES,
                      GLASSMAN_SERVO_ACTIVE_HOLD_MS,
                      GLASSMAN_SERVO_REST_HOLD_MS,
                      GLASSMAN_SERVO_MIN_PULSE_US,
                      GLASSMAN_SERVO_MAX_PULSE_US,
                      GLASSMAN_SERVO_RANGE_DEGREES);

  wdt_reset();
}

void writeServoDegrees(Servo &servo, float degrees, int minPulseUs,
                       int maxPulseUs, float rangeDegrees) {
  const float constrainedDegrees = constrain(degrees, 0.0f, rangeDegrees);
  const int pulseWidthUs = minPulseUs +
                           ((maxPulseUs - minPulseUs) * constrainedDegrees /
                            rangeDegrees);

  servo.writeMicroseconds(pulseWidthUs);
}

void updateButton(unsigned long nowMs) {
  const bool buttonReading = digitalRead(TRIGGER_BUTTON_PIN);

  // Restart the debounce interval every time the raw electrical state changes.
  if (buttonReading != lastButtonReading) {
    lastButtonReading = buttonReading;
    lastButtonReadingChangeMs = nowMs;
  }

  // Accept both press and release only after the new state remains stable for
  // the complete debounce interval.
  if (buttonReading == stableButtonState ||
      (nowMs - lastButtonReadingChangeMs) < BUTTON_DEBOUNCE_MS) {
    return;
  }

  stableButtonState = buttonReading;

  if (stableButtonState == LOW) {
    buttonPressedMs = nowMs;
    buttonPressInProgress = true;
    return;
  }

  if (!buttonPressInProgress) {
    return;
  }

  const unsigned long pressDurationMs = nowMs - buttonPressedMs;
  buttonPressInProgress = false;

  // Ignore this release if either servo has not completed its full sequence.
  // This guarantees that only one servo sequence can run at a time.
  if (isServoSequenceRunning()) {
    return;
  }

  if (pressDurationMs >= GLASSMAN_PRESS_THRESHOLD_MS) {
    startGlassmanSequence(nowMs);
  } else {
    startKepcoSequence(nowMs);
  }
}

bool isServoSequenceRunning() {
  return kepcoSequence.phase != ServoSequencePhase::IDLE ||
         glassmanSequence.phase != ServoSequencePhase::IDLE;
}

void startKepcoSequence(unsigned long nowMs) {
  writeServoDegrees(kepcoServo, KEPCO_SERVO_ACTIVE_DEGREES,
                    KEPCO_SERVO_MIN_PULSE_US, KEPCO_SERVO_MAX_PULSE_US,
                    KEPCO_SERVO_RANGE_DEGREES);
  kepcoSequence.phase = ServoSequencePhase::HOLDING_ACTIVE;
  kepcoSequence.phaseStartedMs = nowMs;
}

void startGlassmanSequence(unsigned long nowMs) {
  writeServoDegrees(glassmanServo, GLASSMAN_SERVO_ACTIVE_DEGREES,
                    GLASSMAN_SERVO_MIN_PULSE_US,
                    GLASSMAN_SERVO_MAX_PULSE_US,
                    GLASSMAN_SERVO_RANGE_DEGREES);
  glassmanSequence.phase = ServoSequencePhase::HOLDING_ACTIVE;
  glassmanSequence.phaseStartedMs = nowMs;
}

void updateServoSequence(Servo &servo, ServoSequenceState &sequence,
                         unsigned long nowMs, float restDegrees,
                         unsigned long activeHoldMs,
                         unsigned long restHoldMs, int minPulseUs,
                         int maxPulseUs, float rangeDegrees) {
  const unsigned long phaseDurationMs = nowMs - sequence.phaseStartedMs;

  if (sequence.phase == ServoSequencePhase::HOLDING_ACTIVE &&
      phaseDurationMs >= activeHoldMs) {
    writeServoDegrees(servo, restDegrees, minPulseUs, maxPulseUs,
                      rangeDegrees);
    sequence.phase = ServoSequencePhase::HOLDING_REST;
    sequence.phaseStartedMs = nowMs;
  } else if (sequence.phase == ServoSequencePhase::HOLDING_REST &&
             phaseDurationMs >= restHoldMs) {
    sequence.phase = ServoSequencePhase::IDLE;
  }
}
