// Logic-Chip Clock: Arduino Nano variant
//
// The Nano replaces board 2 (CD4060, four CD4518, CD4081). Board 1 stays as
// built: the four CD4511 decoders still drive the displays. The Nano puts a
// digit on a shared 4-bit bus and pulses that decoder's LE pin, so each
// CD4511 latches its own digit and holds it. No multiplexing.
//
// Timekeeping: a DS3231 module on A4/A5 is used if one is found. Without it
// the sketch counts on millis(), which drifts (the Nano's resonator can be
// off by several minutes a day) and resets to 00:00 when power is lost.
// Tune MS_PER_SECOND below if you run without the RTC.

#include <Wire.h>

// ---- Pins -----------------------------------------------------------------
const uint8_t BCD_PINS[4] = {2, 3, 4, 5};  // -> A, B, C, D (pins 7, 1, 2, 6) of all four CD4511s
const uint8_t LE_PINS[4]  = {6, 7, 8, 9};  // -> pin 5 (LE) of U6, U7, U8, U9
                                           //    = hours tens, hours ones, minutes tens, minutes ones
const uint8_t DP_PIN      = 10;            // -> R29 (1k) -> DS2 pin 5 (seconds blink)
const uint8_t BTN_HOURS   = 11;            // push button to GND
const uint8_t BTN_MINUTES = 12;            // push button to GND

// ---- Settings -------------------------------------------------------------
const uint8_t  DS3231_ADDR   = 0x68;
const uint32_t MS_PER_SECOND = 1000;  // millis() mode only. Clock gains time? Raise it (e.g. 1003).
const uint16_t DEBOUNCE_MS   = 30;
const uint16_t REPEAT_DELAY  = 600;   // hold a button this long before it auto-repeats
const uint16_t REPEAT_EVERY  = 200;

// ---- State ----------------------------------------------------------------
bool     haveRtc = false;
uint8_t  hh = 0, mm = 0, ss = 0;
uint32_t secondStart = 0;   // millis() when the current second began
uint8_t  shown[4] = {255, 255, 255, 255};

struct Button {
  uint8_t  pin;
  bool     stable;       // true = pressed
  bool     lastRead;
  uint32_t changedAt;
  uint32_t nextRepeat;
};
Button btnH = {BTN_HOURS, false, false, 0, 0};
Button btnM = {BTN_MINUTES, false, false, 0, 0};

// ---- Display --------------------------------------------------------------
void latchDigit(uint8_t idx, uint8_t value) {
  if (shown[idx] == value) return;
  for (uint8_t b = 0; b < 4; b++) {
    digitalWrite(BCD_PINS[b], (value >> b) & 1);
  }
  digitalWrite(LE_PINS[idx], LOW);   // decoder follows the bus
  delayMicroseconds(5);
  digitalWrite(LE_PINS[idx], HIGH);  // rising edge latches the digit
  shown[idx] = value;
}

void showTime() {
  latchDigit(0, hh / 10);
  latchDigit(1, hh % 10);
  latchDigit(2, mm / 10);
  latchDigit(3, mm % 10);
}

// ---- DS3231 ---------------------------------------------------------------
uint8_t bcdToDec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t decToBcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcRead(uint8_t &h, uint8_t &m, uint8_t &s) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(DS3231_ADDR, (uint8_t)3) != 3) return false;
  s = bcdToDec(Wire.read() & 0x7F);
  m = bcdToDec(Wire.read() & 0x7F);
  uint8_t hr = Wire.read();
  if (hr & 0x40) {                       // module was left in 12-hour mode
    uint8_t h12 = bcdToDec(hr & 0x1F);
    bool pm = hr & 0x20;
    h = (h12 % 12) + (pm ? 12 : 0);
  } else {
    h = bcdToDec(hr & 0x3F);
  }
  return h < 24 && m < 60 && s < 60;
}

void rtcWrite(uint8_t h, uint8_t m, uint8_t s) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));               // bit 6 clear = 24-hour mode
  Wire.endTransmission();
}

// ---- Buttons --------------------------------------------------------------
// Returns true once on press, then repeatedly while held.
bool buttonFired(Button &btn, uint32_t now) {
  bool reading = digitalRead(btn.pin) == LOW;
  if (reading != btn.lastRead) {
    btn.lastRead = reading;
    btn.changedAt = now;
  }
  if (now - btn.changedAt < DEBOUNCE_MS) return false;

  if (reading != btn.stable) {
    btn.stable = reading;
    if (reading) {
      btn.nextRepeat = now + REPEAT_DELAY;
      return true;
    }
    return false;
  }
  if (btn.stable && (int32_t)(now - btn.nextRepeat) >= 0) {
    btn.nextRepeat = now + REPEAT_EVERY;
    return true;
  }
  return false;
}

// ---- Main -----------------------------------------------------------------
void setup() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(BCD_PINS[i], OUTPUT);
    pinMode(LE_PINS[i], OUTPUT);
    digitalWrite(LE_PINS[i], HIGH);
  }
  pinMode(DP_PIN, OUTPUT);
  pinMode(BTN_HOURS, INPUT_PULLUP);
  pinMode(BTN_MINUTES, INPUT_PULLUP);

  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);       // don't hang if A4/A5 are left unconnected
#endif
  Wire.beginTransmission(DS3231_ADDR);
  haveRtc = (Wire.endTransmission() == 0);
  if (haveRtc) rtcRead(hh, mm, ss);

  secondStart = millis();
  showTime();
}

void loop() {
  uint32_t now = millis();

  // Advance the clock
  if (haveRtc) {
    static uint32_t lastPoll = 0;
    if (now - lastPoll >= 50) {
      lastPoll = now;
      uint8_t h, m, s;
      if (rtcRead(h, m, s) && s != ss) {
        hh = h; mm = m; ss = s;
        secondStart = now;
      }
    }
  } else {
    while (now - secondStart >= MS_PER_SECOND) {
      secondStart += MS_PER_SECOND;
      if (++ss == 60) {
        ss = 0;
        if (++mm == 60) {
          mm = 0;
          if (++hh == 24) hh = 0;
        }
      }
    }
  }

  // Set buttons: hours button adds an hour, minutes button adds a minute
  // and zeroes the seconds so you can sync to a reference clock.
  bool changed = false;
  if (buttonFired(btnH, now)) { hh = (hh + 1) % 24; changed = true; }
  if (buttonFired(btnM, now)) { mm = (mm + 1) % 60; ss = 0; secondStart = now; changed = true; }
  if (changed && haveRtc) rtcWrite(hh, mm, ss);

  showTime();
  digitalWrite(DP_PIN, (now - secondStart) < 500 ? HIGH : LOW);
}
