#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <arduino-timer.h>

#define STARTUP_DELAY_MS 500

#define BAR_HEIGHT 7

#define SCREEN_WIDTH  128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixel

#define PIN_SHF A7
#define PIN_VHF A2

#define SCREEN_UPDATE_PERIOD_US 1000UL*1000UL
#define MEASURE_PERIOD_US       500UL
#define MEASURE_PERIOD_1M_US    60UL*1000UL*1000UL

#define CALA_TABLE_SIZE 16
#define CALB_TABLE_SIZE 1

#define DEFAULT_VALUE 50

struct cal_t {
  int v;
  int dbm;
};

cal_t calA_[CALA_TABLE_SIZE] = {
  {  0,   -94 },
  { 21,   -54 },
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

int valueA_, valueB_, valueA_1M_, valueB_1M_;
Timer<3, micros> timer_;

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
  delay(STARTUP_DELAY_MS);
  
  display_.clearDisplay();
  display_.display();

  valueA_ = valueB_ = valueA_1M_ = valueB_1M_ = DEFAULT_VALUE;

  timer_.every(MEASURE_PERIOD_1M_US, clean1MValue);
  timer_.every(SCREEN_UPDATE_PERIOD_US, printMeasuredValue);
  timer_.every(MEASURE_PERIOD_US, measure);
}

void loop() {
  timer_.tick();
}

bool measure(void *) {
  int vA = analogRead(PIN_SHF);
  if (vA > valueA_) {
    valueA_ = vA;
  }
  if (vA > valueA_1M_) {
    valueA_1M_ = vA;
  }
  int vB = analogRead(PIN_VHF);
  if (vB > valueB_) {
    valueB_ = vB;
  }
  if (vB > valueB_1M_) {
    valueB_1M_ = vB;
  }
  return true;
}

float toMw(int dbm) {
  return pow(10, (float)dbm / 10.0);
}

int toBarLengthA(int dbm) {
  double k = (double)(SCREEN_WIDTH) / (double)(calA_[CALA_TABLE_SIZE - 2].dbm - calA_[0].dbm);
  return k * (double)(dbm - calA_[2].dbm);
}

int toBarLengthB(int dbm) {
  double k = (double)(SCREEN_WIDTH) / (double)(calB_[CALB_TABLE_SIZE - 2].dbm - calB_[0].dbm);
  return k * (double)(dbm - calB_[0].dbm);
}

int toDbmA(int adValue) {
  int prevValue = calA_[0].v;
  int prevDbm = calA_[0].dbm;
  
  for (int i = 1; i < CALA_TABLE_SIZE; i++) {
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
  int prevValue = calB_[0].v;
  int prevDbm = calB_[0].dbm;
  
  for (int i = 1; i < CALA_TABLE_SIZE; i++) {
   if (adValue >= prevValue && adValue <= calA_[i].v) {
     double k = (double)(calB_[i].dbm - prevDbm) / (double)(calB_[i].v - prevValue);
     return k * (double)(calB_[i].v - prevValue) + prevDbm;
   }
   prevValue = calB_[i].v;
   prevDbm = calB_[i].dbm;
  }
  return calA_[CALA_TABLE_SIZE - 1].dbm;
}

bool clean1MValue(void *) {
  valueA_1M_ = valueB_1M_ = DEFAULT_VALUE;
  return true;
}

bool printMeasuredValue(void *) {
  int dbmA = toDbmA(valueA_);
  int dbmA_1M = toDbmA(valueA_1M_);
  float mwA = toMw(dbmA);
  
  int dbmB = toDbmB(valueB_);
  int dbmB_1M = toDbmA(valueB_1M_);
  float mwB = toMw(dbmB);
  
  display_.clearDisplay();
  display_.setTextSize(1);

  // channel A
  display_.drawRect(0, 0, SCREEN_WIDTH, BAR_HEIGHT, SSD1306_WHITE);
  display_.fillRect(0, 0, toBarLengthA(dbmA), BAR_HEIGHT, SSD1306_WHITE);

  int barA_1M = toBarLengthA(dbmA_1M);
  display_.drawLine(barA_1M, 0, barA_1M, BAR_HEIGHT - 1, SSD1306_WHITE);

  display_.setCursor(0, BAR_HEIGHT + 1);
  display_.print("A: ");
  display_.print(dbmA);
  display_.print("dBm ");
  if (mwA >= 1) {
    display_.print(mwA);
    display_.print("mW ");
  } else {
    display_.print(mwA * 1000);
    display_.print("uW ");
  }

  // channel B
  display_.drawRect(0, 2 * BAR_HEIGHT + 2, SCREEN_WIDTH, BAR_HEIGHT, SSD1306_WHITE);
  display_.fillRect(0, 2 * BAR_HEIGHT + 2, toBarLengthB(dbmB), BAR_HEIGHT, SSD1306_WHITE);

  int barB_1M = toBarLengthA(dbmB_1M);
  display_.drawLine(barB_1M, 2 * BAR_HEIGHT + 2, barB_1M, 3 * BAR_HEIGHT + 1, SSD1306_WHITE);

  display_.setCursor(0, 3 * BAR_HEIGHT + 3);
  display_.print("B: ");
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
  valueA_ = valueB_ = DEFAULT_VALUE; 
  return true;
}
