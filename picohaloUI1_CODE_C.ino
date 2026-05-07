#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_NeoPixel.h>   // ADDED

void drawEyes();
void clearEyes();
void drawBootScreen();

// ===== LOADING SCREEN =====
bool showBootScreen = true;

bool bootDrawn = false;
unsigned long bootStart = 0;

float lastBootProgress = -1; //EXTRA

#define USE_FAHRENHEIT false
// ====== PIN SETUP ======
#define TFT_CS   20
#define TFT_DC   17
#define TFT_RST  16
#define TFT_SCK  18
#define TFT_MOSI 19

#define TOUCH_PIN 22
#define LED_PIN   15

#define I2C_SDA 4
#define I2C_SCL 5

// ====== DISPLAY OBJECT ======
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ====== SENSOR OBJECTS ======
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// ====== LED RING ======
#define NUM_LEDS 16
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ====== PAGE CONTROL ======
bool sensorPage = false;
bool lastTouchState = LOW;

// ====== SENSOR TIMING ======
unsigned long lastSensorUpdate = 0;
unsigned long lastDisplayUpdate = 0;   // ⭐ ADDED (FIX)

float displayTemp = 0;
float displayHum = 0;
float displayPres = 0;

// ====== ADDED GLOBALS (STEP 3) ======
float lastTemp = 0;
float lastHum = 0;
float lastPres = 0;

bool showNotification = false;
unsigned long notificationStart = 0;
String notificationMsg = "";
uint16_t notificationColor = GC9A01A_WHITE;

int ledOffset = 0;

// SAFE RANGES
#define TEMP_MIN 18
#define TEMP_MAX 28
#define HUM_MIN 30
#define HUM_MAX 70
#define PRES_MIN 950
#define PRES_MAX 1050

// ====== COLORS ======
#define BG_COLOR   GC9A01A_BLACK
#define EYE_COLOR  GC9A01A_WHITE
#define TEXT_COLOR GC9A01A_CYAN
#define GC9A01A_RED   0xF800
#define GC9A01A_BLUE  0x001F

// ====== EYE CONFIG ======
int eyeW = 40;
int eyeH = 100;
int radius = 20;

int leftX  = 60;
int rightX = 140;
int eyeY   = 70;

// ====== TIMING ======
unsigned long lastBlink = 0;
unsigned long blinkDelay = 0;

bool blinking = false;
unsigned long blinkStart = 0;

unsigned long lastMsg = 0;
unsigned long msgDelay = 0;

bool showMsg = false;
unsigned long msgStart = 0;

// ====== SENSOR PAGE FIX ======
bool forceSensorDraw = false;

// ====== MESSAGES ======
const char* msgs[] = {
  "Cool!",
  "Let's do a project!",
  "You are doing great!",
  "Me on a Pico!",
  "Let's 3D print!"
};

int msgCount = 5;

// ====== LED RAINBOW STATE ======
uint16_t rainbowHue = 0;

void drawBootScreen() {

  unsigned long now = millis();
  unsigned long elapsed = now - bootStart;

  float progress = (float)elapsed / 5000.0;
  if (progress > 1.0) progress = 1.0;

  if (!bootDrawn) {

    tft.fillScreen(BG_COLOR);

    tft.setTextSize(2);
    tft.setTextColor(GC9A01A_WHITE);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds("Pico HALO v1", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, 20);
    tft.print("Pico HALO v1");

    tft.setTextSize(1);
    tft.getTextBounds("PicohaloUI1 starting...", 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, 110);
    tft.print("PicohaloUI1 starting...");

    tft.drawRect(40, 140, 160, 15, GC9A01A_WHITE);

    bootDrawn = true;
  }

    static int lastFillW = -1;

    int fillW = (160 - 4) * progress;

  if (fillW != lastFillW) {
    tft.fillRect(42, 142, 156, 11, BG_COLOR);
    tft.fillRect(42, 142, fillW, 11, GC9A01A_WHITE);
    lastFillW = fillW;
}

  for (int i = 0; i < NUM_LEDS; i++) {
    int hue = (elapsed * 10) + (i * 500);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
  }
  strip.show();

  if (elapsed >= 5000 && showBootScreen) {
    showBootScreen = false;

    bootDrawn = false;

    tft.fillScreen(BG_COLOR);

    drawEyes();
    lastBlink = millis();

    return;
  }
}

// ====== LEDS ======

void updateLEDs() {

  if (!showNotification) {
    rainbowHue += 2;

    for (int i = 0; i < NUM_LEDS; i++) {
      int hue = rainbowHue + (i * 300);
      uint32_t color = strip.gamma32(strip.ColorHSV(hue));
      strip.setPixelColor(i, color);
    }

    strip.show();
    return;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (notificationColor == GC9A01A_RED)
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    else
      strip.setPixelColor(i, strip.Color(0, 0, 255));
  }

  strip.show();
}

// ====== RANGE CHECK ======

void checkRanges() {
  showNotification = false;

  if (lastTemp > TEMP_MAX) {
    showNotification = true;
    notificationMsg = "Temp too High!";
    notificationColor = GC9A01A_RED;
  } else if (lastTemp < TEMP_MIN) {
    showNotification = true;
    notificationMsg = "Temp too Low!";
    notificationColor = GC9A01A_BLUE;
  }

  else if (lastHum > HUM_MAX) {
    showNotification = true;
    notificationMsg = "Hum too High!";
    notificationColor = GC9A01A_RED;
  } else if (lastHum < HUM_MIN) {
    showNotification = true;
    notificationMsg = "Hum too Low!";
    notificationColor = GC9A01A_BLUE;
  }

  else if (lastPres > PRES_MAX) {
    showNotification = true;
    notificationMsg = "Pres too High!";
    notificationColor = GC9A01A_RED;
  } else if (lastPres < PRES_MIN) {
    showNotification = true;
    notificationMsg = "Pres too Low!";
    notificationColor = GC9A01A_BLUE;
  }

  if (showNotification) notificationStart = millis();
}

// ====== NOTIFICATION ======

void drawNotification() {
  if (!showNotification) return;

  if (millis() - notificationStart > 120000) {
    showNotification = false;
    tft.fillRect(40, 180, 160, 40, BG_COLOR);
    return;
  }

  tft.drawRoundRect(40, 180, 160, 40, 10, notificationColor);

  tft.setTextColor(notificationColor, BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(50, 200);
  tft.print(notificationMsg);
}

// ====== EYES ======

void drawEyes() {
  tft.fillScreen(BG_COLOR);

  tft.fillRoundRect(leftX, eyeY, eyeW, eyeH, radius, EYE_COLOR);
  tft.fillRoundRect(rightX, eyeY, eyeW, eyeH, radius, EYE_COLOR);
}

void clearEyes() {
  tft.fillRect(leftX, eyeY, eyeW, eyeH, BG_COLOR);
  tft.fillRect(rightX, eyeY, eyeW, eyeH, BG_COLOR);
}

// ====== MESSAGE ======

void showMessage() {
  tft.fillScreen(BG_COLOR);

  int i = random(0, msgCount);

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);

  int16_t x1, y1;
  uint16_t w, h;

  tft.getTextBounds(msgs[i], 0, 0, &x1, &y1, &w, &h);

  int x = (240 - w) / 2;
  int y = (240 - h) / 2;

  tft.setCursor(x, y);
  tft.print(msgs[i]);
}

// ====== SENSOR PAGE ======

void drawSensorPage(float t1, float t2, float hum, float pres) {

  static bool firstDraw = true;

  int boxW = 140;
  int boxH = 45;
  int centerX = 120 - boxW / 2;

  int tempY = 20;
  int humY  = 85;
  int presY = 150;

  if (forceSensorDraw) {
    firstDraw = true;
    forceSensorDraw = false;
  }

  if (firstDraw) {
    tft.fillScreen(BG_COLOR);

    tft.drawRoundRect(centerX, tempY, boxW, boxH, 10, GC9A01A_BLUE);
    tft.drawRoundRect(centerX, humY,  boxW, boxH, 10, GC9A01A_GREEN);
    tft.drawRoundRect(centerX, presY, boxW, boxH, 10, GC9A01A_RED);

    firstDraw = false;
  }

  tft.setTextSize(2);

  tft.fillRect(centerX + 5, tempY + 5, boxW - 10, boxH - 10, BG_COLOR);
  tft.fillRect(centerX + 5, humY  + 5, boxW - 10, boxH - 10, BG_COLOR);
  tft.fillRect(centerX + 5, presY + 5, boxW - 10, boxH - 10, BG_COLOR);

  tft.setTextColor(GC9A01A_WHITE, BG_COLOR);

  tft.setCursor(centerX + 10, tempY + 10);
  tft.print("Temp:");
  tft.setCursor(centerX + 10, tempY + 25);
  float tempToShow = USE_FAHRENHEIT ? (t1 * 9.0 / 5.0 + 32) : t1;

  tft.print(tempToShow, 1);
  tft.print(USE_FAHRENHEIT ? " F" : " C");

  tft.setCursor(centerX + 10, humY + 10);
  tft.print("Hum:");
  tft.setCursor(centerX + 10, humY + 25);
  tft.print(hum, 1);
  tft.print(" %");

  tft.setCursor(centerX + 10, presY + 10);
  tft.print("Pres:");
  tft.setCursor(centerX + 10, presY + 25);
  tft.print(pres, 1);
  tft.print(" hPa");
}

// ====== SETUP ======

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  strip.begin();
  strip.show();

  SPI.setSCK(TFT_SCK);
  SPI.setTX(TFT_MOSI);

  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  aht.begin();
  bmp.begin(0x77);

  tft.begin();
  tft.setRotation(0);
  bootStart = millis();

  blinkDelay = random(3000, 6000);
  msgDelay   = random(60000, 180000);
}

// ====== LOOP ======

void loop() {

  if (showBootScreen) {
    drawBootScreen();
    return;
  }

  unsigned long now = millis();

  if (now - lastSensorUpdate >= 3000) {
    lastSensorUpdate = now;

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    lastTemp = temp.temperature;
    lastHum  = humidity.relative_humidity;
    lastPres = bmp.readPressure() / 100.0F;

    checkRanges();
  }

  updateLEDs();

  bool touchState = digitalRead(TOUCH_PIN);

  if (touchState == HIGH && lastTouchState == LOW) {
    sensorPage = !sensorPage;
    tft.fillScreen(BG_COLOR);

    if (!sensorPage) drawEyes();
    else {
      forceSensorDraw = true;
      lastDisplayUpdate = 0;
    }
  }

  lastTouchState = touchState;

  if (sensorPage) {
    if (now - lastDisplayUpdate >= 5000) {
      lastDisplayUpdate = now;

      drawSensorPage(lastTemp,
                     bmp.readTemperature(),
                     lastHum,
                     lastPres);
    }
    return;
  }

  drawNotification();

  if (showMsg) {
    if (now - msgStart >= 5000) {
      showMsg = false;
      drawEyes();

      lastMsg = now;
      msgDelay = random(60000, 180000);
    }
    return;
  }

  if (now - lastMsg >= msgDelay) {
    showMsg = true;
    msgStart = now;
    showMessage();
    return;
  }

  if (!blinking && (now - lastBlink >= blinkDelay)) {
    blinking = true;
    blinkStart = now;
    clearEyes();
  }

  if (blinking && (now - blinkStart >= 200)) {
    blinking = false;
    drawEyes();

    lastBlink = now;
    blinkDelay = random(3000, 6000);
  }
}