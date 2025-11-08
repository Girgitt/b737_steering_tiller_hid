#include <Arduino.h>
#include <Joystick.h>

// ====== CONFIG ======
static const uint8_t POT_PIN = A3;

// ADC on 32u4 is 10-bit: 0..1023
static const int ADC_MIN = 0;
static const int ADC_MAX = 1023;

// Smoothing: 0..1 (as float). 0.2 = quite responsive, still smooth
static const float EMA_ALPHA = 0.2f;

// Only send new HID report if value changed at least this much
static const int SEND_DEADZONE = 2;

// How often to sample (ms)
static const uint16_t SAMPLE_INTERVAL_MS = 5;

// ====== Joystick object ======
// We'll expose ONLY X axis, no buttons, no others.
Joystick_ Joystick(
    JOYSTICK_DEFAULT_REPORT_ID,
    JOYSTICK_TYPE_GAMEPAD,
    0,  // button count
    0,  // hat switch count
    true,  // X axis
    false, // Y
    false, // Z
    false, // Rx
    false, // Ry
    false, // Rz
    false, // rudder
    false, // throttle
    false, // accelerator
    false, // brake
    false  // steering
);

// ----- helpers -----

// Small helper: return median of 3 ints
int median3(int a, int b, int c) {
    if ((a >= b && a <= c) || (a >= c && a <= b)) return a;
    if ((b >= a && b <= c) || (b >= c && b <= a)) return b;
    return c;
}

void setup() {
    // Make sure the ADC pin is input (it is by default, but let's be explicit)
    pinMode(POT_PIN, INPUT);

    // Tell joystick library what range we’ll be sending
    Joystick.setXAxisRange(ADC_MIN, ADC_MAX);
    Joystick.begin();

    // Optional: initial read to “warm up” filtering
    int raw0 = analogRead(POT_PIN);
    Joystick.setXAxis(raw0);
}

void loop() {
    static uint32_t lastSampleMs = 0;
    static float emaValue = 0.0f;
    static int lastSent = -9999;

    uint32_t now = millis();
    if (now - lastSampleMs < SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSampleMs = now;

    // ----- 1) read 3 times and median -----
    int r1 = analogRead(POT_PIN);
    int r2 = analogRead(POT_PIN);
    int r3 = analogRead(POT_PIN);
    int raw = median3(r1, r2, r3);

    // ----- 2) first run: init EMA with raw -----
    if (emaValue == 0.0f && raw != 0) {
        emaValue = raw;
    } else {
        // EMA: new = alpha * raw + (1-alpha) * old
        emaValue = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * emaValue;
    }

    int filtered = (int)(emaValue + 0.5f);  // round

    // ----- 3) send only if changed enough -----
    if (abs(filtered - lastSent) >= SEND_DEADZONE) {
        Joystick.setXAxis(filtered);
        lastSent = filtered;
    }
}

