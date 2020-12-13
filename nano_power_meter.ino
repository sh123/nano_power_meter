#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduino-timer.h>

#define STARTUP_DELAY 1000
#define BAR_HEIGHT 7
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixel
#define PIN_SHF A7
#define PIN_VHF A6
#define SCREEN_UPDATE_PERIOD 1000
#define MEASURE_PERIOD 1
#define CALA_TABLE_SIZE 14
#define CALB_TABLE_SIZE 1

struct cal_t {
  int v;
  int dbm;
};

cal_t calA_[CALA_TABLE_SIZE] = {
  { 50,   -32 },
  { 56,   -28 },
  { 59,   -24 },
  { 63,   -20 },
  { 72,   -16 },
  { 102,  -12 },
  { 122,   -8 },
  { 143,   -4 },
  { 163,    0 },
  { 204,    4 },
  { 245,    8 },
  { 409,   12 },
  { 614,   20 },
  { 1024,  30 }
};

// TODO, calibrate for HF/VHF channel B
cal_t calB_[CALB_TABLE_SIZE] = {
  { 50,   -32 }
};

int valueA_, valueB_;
auto timer_ = timer_create_default();

Adafruit_SSD1306 display_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  if(!display_.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display_.clearDisplay();
  display_.setTextSize(2);      // Normal 1:1 pixel scale
  display_.setTextColor(SSD1306_WHITE); // Draw white text
  display_.setCursor(8, 8);     // Start at top-left corner
  display_.print(F("PWR meter"));
  display_.display();
  delay(STARTUP_DELAY);
  
  display_.clearDisplay();
  display_.display();

  valueA_ = valueB_ = 50;
  
  timer_.every(SCREEN_UPDATE_PERIOD, printMeasuredValue);
  timer_.every(MEASURE_PERIOD, measure);
}

void loop() {
  timer_.tick();
}

float toMw(int dbm) {
  return pow(10, (float)dbm / 10.0);
}

int toBarLengthA(int dbm) {
  double k = (double)(SCREEN_WIDTH) / (double)(calA_[CALA_TABLE_SIZE - 2].dbm - calA_[0].dbm);
  return k * (double)(dbm - calA_[0].dbm);
}

int toBarLengthB(int dbm) {
  double k = (double)(SCREEN_WIDTH) / (double)(calB_[CALB_TABLE_SIZE - 2].dbm - calB_[0].dbm);
  return k * (double)(dbm - calB_[0].dbm);
}

int toDbmA(int adValue) {
  int prevValue = 1;
  int prevDbm = -50;
  
  for (int i = 0; i < CALA_TABLE_SIZE; i++) {
   if (adValue >= prevValue && adValue <= calA_[i].v) {
     double k = (double)(calA_[i].dbm - prevDbm) / (double)(calA_[i].v - prevValue);
     return k * (double)(calA_[i].v - prevValue) + prevDbm;
   }
   prevValue = calA_[i].v;
   prevDbm = calA_[i].dbm;
  }
  return calA_[CALA_TABLE_SIZE - 1].dbm;
}

int toDbmB(int adValue) {
  int prevValue = 1;
  int prevDbm = -50;
  
  for (int i = 0; i < CALA_TABLE_SIZE; i++) {
   if (adValue >= prevValue && adValue <= calA_[i].v) {
     double k = (double)(calB_[i].dbm - prevDbm) / (double)(calB_[i].v - prevValue);
     return k * (double)(calB_[i].v - prevValue) + prevDbm;
   }
   prevValue = calB_[i].v;
   prevDbm = calB_[i].dbm;
  }
  return calA_[CALA_TABLE_SIZE - 1].dbm;
}

bool measure(void *) {
  int vA = analogRead(PIN_SHF);
  if (vA > valueA_) {
    valueA_ = vA;
  }
  return true;
}

bool printMeasuredValue(void *) {
  int dbmA = toDbmA(valueA_);
  float mwA = toMw(dbmA);
  int dbmB = toDbmB(valueB_);
  float mwB = toMw(dbmB);
  
  display_.clearDisplay();
  display_.setTextSize(1);

  display_.drawRect(0, 0, SCREEN_WIDTH, BAR_HEIGHT, SSD1306_WHITE);
  display_.fillRect(0, 0, toBarLengthA(dbmA), BAR_HEIGHT, SSD1306_WHITE);

  display_.setCursor(0, BAR_HEIGHT + 1);

  display_.print(dbmA);
  display_.print("dBm ");
  if (mwA >= 1) {
    display_.print(mwA);
    display_.print("mW ");
  } else {
    display_.print(mwA * 1000);
    display_.print("uW ");
  }
  
  display_.drawRect(0, 2 * BAR_HEIGHT + 2, SCREEN_WIDTH, BAR_HEIGHT, SSD1306_WHITE);
  display_.fillRect(0, 2 * BAR_HEIGHT + 2, toBarLengthB(dbmB), BAR_HEIGHT, SSD1306_WHITE);

  display_.setCursor(0, 3 * BAR_HEIGHT + 3);

  display_.print(dbmB);
  display_.print("dBm ");
  if (mwB >= 1) {
    display_.print(mwB);
    display_.print("mW ");
  } else {
    display_.print(mwB * 1000);
    display_.print("uW ");
  }
  
  display_.display();
  valueA_ = 0; 
  return true;
}
