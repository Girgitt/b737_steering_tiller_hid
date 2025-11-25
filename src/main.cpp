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
static const int SEND_DEADZONE = 1;

// How often to sample (ms)
static const uint16_t SAMPLE_INTERVAL_MS = 5;


int16_t zero_offset_raw = 0;

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
Joystick.setXAxisRange(0, 1023);

    Joystick.begin();
    delay(500);

    // Optional: initial read to “warm up” filtering
    int r1 = analogRead(POT_PIN);
    int r2 = analogRead(POT_PIN);
    int r3 = analogRead(POT_PIN);
    int raw = median3(r1, r2, r3);

    zero_offset_raw = raw;
    Joystick.setXAxis(512);
}

void loop() {
    static uint32_t lastSampleMs = 0;
    static float emaValue = 0.0f;
    static int lastSent = -9999;

    static int minRaw = 1023, maxRaw = 0;

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

    if (filtered < minRaw) minRaw = filtered;
    if (filtered > maxRaw) maxRaw = filtered;

    int delta = filtered - (int)zero_offset_raw;

    int spanNeg = max(1, (int)zero_offset_raw - minRaw);
    int spanPos = max(1, maxRaw - (int)zero_offset_raw);

    int out;
    if (delta >= 0) {
    // right side: 512..1023
    out = 512 + (int)lroundf((float)delta * 511.0f / (float)spanPos);
    } else {
    // left side: 0..512
    out = 512 - (int)lroundf((float)(-delta) * 512.0f / (float)spanNeg);
    }
    out = constrain(out, 0, 1023);

    int diff = abs(out - lastSent);
    bool centerKick = (lastSent >= 508 && lastSent <= 516) && (diff >= 1);

    if (abs(out - lastSent) >= SEND_DEADZONE || centerKick) {
    Joystick.setXAxis(out);
    lastSent = out;
    }
}

