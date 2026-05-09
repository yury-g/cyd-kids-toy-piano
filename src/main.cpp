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
    {"C", "home", 523, 255, 64, 64, 255, 0, 0, 8, 58, 56, 118},
    {"D", "step", 587, 255, 160, 40, 255, 90, 0, 70, 58, 56, 118},
    {"E", "bright", 659, 250, 235, 50, 130, 255, 0, 132, 58, 56, 118},
    {"G", "lift", 784, 70, 210, 120, 0, 255, 0, 194, 58, 56, 118},
    {"A", "spark", 880, 80, 145, 255, 0, 80, 255, 256, 58, 56, 118},
};

String devLines[] = {"booting", "", ""};
int8_t activeKey = -1;
uint16_t currentFrequency = 0;
uint16_t currentDuty = 0;

uint16_t makeColor(uint8_t red, uint8_t green, uint8_t blue) {
  return tft.color565(red, green, blue);
}

uint16_t complementColor(const PianoKey &key) {
  return makeColor(255 - key.red, 255 - key.green, 255 - key.blue);
}

bool shouldUseDarkText(const PianoKey &key) {
  return key.red + key.green + key.blue > 420;
}

void addDevMessage(const String &message) {
  devLines[2] = devLines[1];
  devLines[1] = devLines[0];
  devLines[0] = message;

  tft.fillRect(0, 199, SCREEN_WIDTH, 41, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("DEV", 8, 205, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  for (uint8_t i = 0; i < 3; i++) {
    tft.drawString(devLines[i], 48, 205 + (i * 11), 1);
  }

  Serial.println(message);
}

void setRgbLed(uint8_t red, uint8_t green, uint8_t blue) {
  ledcWrite(LED_RED_CHANNEL, 255 - red);
  ledcWrite(LED_GREEN_CHANNEL, 255 - green);
  ledcWrite(LED_BLUE_CHANNEL, 255 - blue);
}

void drawHeader() {
  tft.fillRect(0, 0, SCREEN_WIDTH, 42, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Color Piano", 10, 6, 4);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("5 happy notes", 210, 11, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Tap keys: sound + matching LED color", 10, 31, 1);
}

void drawKey(uint8_t index, bool pressed) {
  const PianoKey &key = keys[index];
  const uint16_t fill = makeColor(key.red, key.green, key.blue);
  const uint16_t comp = complementColor(key);
  const uint16_t text = shouldUseDarkText(key) ? TFT_BLACK : TFT_WHITE;
  const int16_t y = pressed ? key.y + 7 : key.y;
  const int16_t h = pressed ? key.h - 7 : key.h;

  tft.fillRect(key.x - 2, key.y - 2, key.w + 4, key.h + 10, TFT_NAVY);
  tft.fillRoundRect(key.x, y, key.w, h, 6, fill);
  tft.drawRoundRect(key.x, y, key.w, h, 6, pressed ? TFT_WHITE : comp);
  tft.drawRoundRect(key.x + 3, y + 3, key.w - 6, h - 6, 5, comp);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(text, fill);
  tft.drawString(key.note, key.x + key.w / 2, y + 38, 6);
  tft.drawString(key.hint, key.x + key.w / 2, y + 80, 2);

  tft.fillRoundRect(key.x + 9, y + h - 22, key.w - 18, 14, 5, comp);
}

void drawLesson() {
  tft.fillRect(0, 180, SCREEN_WIDTH, 19, TFT_NAVY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);

  if (activeKey >= 0) {
    const PianoKey &key = keys[activeKey];
    tft.drawString(String(key.note) + " note uses a color and its opposite.",
                   10, 182, 2);
  } else {
    tft.drawString("Complementary colors are color opposites.", 10, 182, 2);
  }
}

uint16_t pressureToDuty(int16_t z) {
  return constrain(map(z, PRESSURE_MIN_Z, PRESSURE_MAX_Z, MIN_TONE_DUTY,
                       MAX_TONE_DUTY),
                   MIN_TONE_DUTY, MAX_TONE_DUTY);
}

void drawUi() {
  tft.fillScreen(TFT_NAVY);
  drawHeader();
  for (uint8_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    drawKey(i, false);
  }
  drawLesson();
  addDevMessage("ready: tap a piano key");
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

void stopNote() {
  if (activeKey >= 0) {
    drawKey(activeKey, false);
    activeKey = -1;
    drawLesson();
  }

  currentFrequency = 0;
  currentDuty = 0;
  ledcWrite(SPEAKER_CHANNEL, 0);
  ledcWriteTone(SPEAKER_CHANNEL, 0);
}

void playKey(uint8_t index, int16_t z) {
  const uint16_t duty = pressureToDuty(z);

  if (activeKey == index) {
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
  currentDuty = duty;
  setRgbLed(key.ledRed, key.ledGreen, key.ledBlue);
  ledcWriteTone(SPEAKER_CHANNEL, currentFrequency);
  ledcWrite(SPEAKER_CHANNEL, currentDuty);

  drawKey(index, true);
  drawLesson();
  addDevMessage(String("note ") + key.note + " tone=" + currentFrequency +
                " vol=" + currentDuty);
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

  Serial.println();
  Serial.println("CYD Color Piano is running.");
}

void loop() {
  static uint32_t lastTouchMs = 0;
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;

  if (readTouchPoint(x, y, z)) {
    if (millis() - lastTouchMs > 70) {
      lastTouchMs = millis();
      const int8_t touchedKey = keyAt(x, y);
      if (touchedKey >= 0) {
        playKey(touchedKey, z);
      } else {
        if (activeKey >= 0) {
          stopNote();
        }
        addDevMessage(String("touch x=") + x + " y=" + y + " z=" + z);
      }
    }
  } else if (activeKey >= 0) {
    stopNote();
  }
}
