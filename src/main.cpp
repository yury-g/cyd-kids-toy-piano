#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen_TT.h>

constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;

constexpr int TOUCH_IRQ = 36;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_SCLK = 25;
constexpr int TOUCH_CS = 33;

constexpr int LED_RED_PIN = 4;
constexpr int LED_GREEN_PIN = 16;
constexpr int LED_BLUE_PIN = 17;
constexpr int LED_RED_CHANNEL = 0;
constexpr int LED_GREEN_CHANNEL = 1;
constexpr int LED_BLUE_CHANNEL = 2;
constexpr int LED_PWM_FREQ = 5000;
constexpr int LED_PWM_BITS = 8;

constexpr int SPEAKER_PIN = 26;
constexpr int SPEAKER_CHANNEL = 6;
constexpr int SPEAKER_BITS = 10;
constexpr int PRESSURE_MIN_Z = 350;
constexpr int PRESSURE_MAX_Z = 1800;
constexpr int MIN_TONE_DUTY = 34;
constexpr int MAX_TONE_DUTY = 155;
constexpr int VOLUME_MIN = 0;
constexpr int VOLUME_MAX = 30;
constexpr int VOL_MINUS_X = 226;
constexpr int VOL_VALUE_X = 254;
constexpr int VOL_PLUS_X = 292;
constexpr int VOL_Y = 5;
constexpr int VOL_BUTTON_SIZE = 22;

constexpr int TOUCH_MIN_X = 200;
constexpr int TOUCH_MAX_X = 3700;
constexpr int TOUCH_MIN_Y = 240;
constexpr int TOUCH_MAX_Y = 3800;

struct PianoKey {
  const char *note;
  const char *hint;
  uint16_t frequency;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t ledRed;
  uint8_t ledGreen;
  uint8_t ledBlue;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSpi = SPIClass(HSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

PianoKey keys[] = {
    {"C", "home", 523, 255, 64, 64, 255, 0, 0, 3, 36, 60, 202},
    {"D", "step", 587, 255, 160, 40, 255, 90, 0, 67, 36, 60, 202},
    {"E", "bright", 659, 250, 235, 50, 130, 255, 0, 131, 36, 60, 202},
    {"G", "lift", 784, 70, 210, 120, 0, 255, 0, 195, 36, 60, 202},
    {"A", "spark", 880, 80, 145, 255, 0, 80, 255, 259, 36, 58, 202},
};

int8_t activeKey = -1;
uint16_t currentFrequency = 0;
uint16_t currentPressureDuty = 0;
uint16_t currentDuty = 0;
uint8_t masterVolume = 4;

uint16_t makeColor(uint8_t red, uint8_t green, uint8_t blue) {
  return tft.color565(red, green, blue);
}

uint16_t complementColor(const PianoKey &key) {
  return makeColor(255 - key.red, 255 - key.green, 255 - key.blue);
}

bool shouldUseDarkText(const PianoKey &key) {
  return key.red + key.green + key.blue > 420;
}

void setRgbLed(uint8_t red, uint8_t green, uint8_t blue) {
  ledcWrite(LED_RED_CHANNEL, 255 - red);
  ledcWrite(LED_GREEN_CHANNEL, 255 - green);
  ledcWrite(LED_BLUE_CHANNEL, 255 - blue);
}

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_WIDTH, 32, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Color Piano", 8, 6, 4);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("tap a key", 142, 14, 2);
}

void drawVolumeControl() {
  char volumeText[5];
  snprintf(volumeText, sizeof(volumeText), "%u", masterVolume);

  tft.fillRect(198, 0, 122, 32, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("VOL", 202, 11, 2);

  tft.fillRoundRect(VOL_MINUS_X, VOL_Y, VOL_BUTTON_SIZE, VOL_BUTTON_SIZE, 4,
                    TFT_DARKGREY);
  tft.fillRoundRect(VOL_PLUS_X, VOL_Y, VOL_BUTTON_SIZE, VOL_BUTTON_SIZE, 4,
                    TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("-", VOL_MINUS_X + VOL_BUTTON_SIZE / 2,
                 VOL_Y + VOL_BUTTON_SIZE / 2, 4);
  tft.drawString("+", VOL_PLUS_X + VOL_BUTTON_SIZE / 2,
                 VOL_Y + VOL_BUTTON_SIZE / 2, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(volumeText, VOL_VALUE_X + 16, VOL_Y + VOL_BUTTON_SIZE / 2, 2);
}

void drawKey(uint8_t index, bool pressed) {
  const PianoKey &key = keys[index];
  const uint16_t fill = makeColor(key.red, key.green, key.blue);
  const uint16_t comp = complementColor(key);
  const uint16_t text = shouldUseDarkText(key) ? TFT_BLACK : TFT_WHITE;
  const int16_t y = pressed ? key.y + 8 : key.y;
  const int16_t h = pressed ? key.h - 8 : key.h;

  tft.fillRect(key.x - 2, key.y - 2, key.w + 4, key.h + 10, TFT_NAVY);
  tft.fillRoundRect(key.x, y, key.w, h, 6, fill);
  tft.drawRoundRect(key.x, y, key.w, h, 6, pressed ? TFT_WHITE : comp);
  tft.drawRoundRect(key.x + 3, y + 3, key.w - 6, h - 6, 5, comp);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(text, fill);
  tft.drawString(key.note, key.x + key.w / 2, y + 76, 7);
  tft.drawString(key.hint, key.x + key.w / 2, y + 126, 2);

  tft.fillRoundRect(key.x + 9, y + h - 22, key.w - 18, 14, 5, comp);
}

uint16_t pressureToRawDuty(int16_t z) {
  return constrain(map(z, PRESSURE_MIN_Z, PRESSURE_MAX_Z, MIN_TONE_DUTY,
                       MAX_TONE_DUTY),
                   MIN_TONE_DUTY, MAX_TONE_DUTY);
}

uint16_t scaleDutyForVolume(uint16_t pressureDuty) {
  return (pressureDuty * masterVolume) / VOLUME_MAX;
}

void drawUi() {
  tft.fillScreen(TFT_NAVY);
  drawHeader();
  drawVolumeControl();
  for (uint8_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    drawKey(i, false);
  }
}

bool readTouchPoint(int16_t &x, int16_t &y, int16_t &z) {
  if (!touch.touched()) {
    return false;
  }

  TS_Point point = touch.getPoint();
  x = constrain(map(point.x, TOUCH_MIN_X, TOUCH_MAX_X, 1, SCREEN_WIDTH), 0,
                SCREEN_WIDTH - 1);
  y = constrain(map(point.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 1, SCREEN_HEIGHT), 0,
                SCREEN_HEIGHT - 1);
  z = point.z;
  return true;
}

int8_t keyAt(int16_t x, int16_t y) {
  for (uint8_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    const PianoKey &key = keys[i];
    if (x >= key.x && x <= key.x + key.w && y >= key.y &&
        y <= key.y + key.h) {
      return i;
    }
  }

  return -1;
}

bool handleVolumeTouch(int16_t x, int16_t y) {
  if (y < 0 || y > 34) {
    return false;
  }

  if (x >= VOL_MINUS_X - 8 && x <= VOL_MINUS_X + VOL_BUTTON_SIZE + 8) {
    const uint8_t step = masterVolume <= 10 ? 1 : 5;
    masterVolume = masterVolume < step ? VOLUME_MIN : masterVolume - step;
  } else if (x >= VOL_PLUS_X - 8 && x <= VOL_PLUS_X + VOL_BUTTON_SIZE + 8) {
    const uint8_t step = masterVolume < 10 ? 1 : 5;
    masterVolume = min<uint8_t>(VOLUME_MAX, masterVolume + step);
  } else {
    return false;
  }

  drawVolumeControl();
  if (activeKey >= 0) {
    currentDuty = scaleDutyForVolume(currentPressureDuty);
    ledcWrite(SPEAKER_CHANNEL, currentDuty);
  }
  return true;
}

void stopNote() {
  if (activeKey >= 0) {
    drawKey(activeKey, false);
    activeKey = -1;
  }

  currentFrequency = 0;
  currentPressureDuty = 0;
  currentDuty = 0;
  ledcWrite(SPEAKER_CHANNEL, 0);
  ledcWriteTone(SPEAKER_CHANNEL, 0);
}

void playKey(uint8_t index, int16_t z) {
  const uint16_t pressureDuty = pressureToRawDuty(z);
  const uint16_t duty = scaleDutyForVolume(pressureDuty);

  if (activeKey == index) {
    currentPressureDuty = pressureDuty;
    currentDuty = duty;
    ledcWrite(SPEAKER_CHANNEL, currentDuty);
    return;
  }

  if (activeKey >= 0) {
    drawKey(activeKey, false);
  }

  activeKey = index;
  const PianoKey &key = keys[index];
  currentFrequency = key.frequency;
  currentPressureDuty = pressureDuty;
  currentDuty = duty;
  setRgbLed(key.ledRed, key.ledGreen, key.ledBlue);
  ledcWriteTone(SPEAKER_CHANNEL, currentFrequency);
  ledcWrite(SPEAKER_CHANNEL, currentDuty);

  drawKey(index, true);
}

void setupLed() {
  ledcSetup(LED_RED_CHANNEL, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LED_GREEN_CHANNEL, LED_PWM_FREQ, LED_PWM_BITS);
  ledcSetup(LED_BLUE_CHANNEL, LED_PWM_FREQ, LED_PWM_BITS);
  ledcAttachPin(LED_RED_PIN, LED_RED_CHANNEL);
  ledcAttachPin(LED_GREEN_PIN, LED_GREEN_CHANNEL);
  ledcAttachPin(LED_BLUE_PIN, LED_BLUE_CHANNEL);
  setRgbLed(0, 0, 0);
}

void setupSpeaker() {
  ledcSetup(SPEAKER_CHANNEL, 523, SPEAKER_BITS);
  ledcAttachPin(SPEAKER_PIN, SPEAKER_CHANNEL);
  ledcWrite(SPEAKER_CHANNEL, 0);
}

void setup() {
  Serial.begin(115200);
  delay(200);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  tft.init();
  tft.setRotation(1);
  touchSpi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);
  setupLed();
  setupSpeaker();
  drawUi();
}

void loop() {
  static uint32_t lastTouchMs = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;

  if (readTouchPoint(x, y, z)) {
    if (millis() - lastTouchMs > 70) {
      lastTouchMs = millis();
      if (handleVolumeTouch(x, y)) {
        return;
      }
      const int8_t touchedKey = keyAt(x, y);
      if (touchedKey >= 0) {
        playKey(touchedKey, z);
      } else {
        if (activeKey >= 0) {
          stopNote();
        }
      }
    }
  } else if (activeKey >= 0) {
    stopNote();
  }
}
