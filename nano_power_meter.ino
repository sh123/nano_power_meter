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
#define PIN_BUZZER 2

#define SCREEN_UPDATE_PERIOD_US 1000UL*1000UL
#define MEASURE_PERIOD_US       500UL
#define MEASURE_PERIOD_1M_US    60UL*1000UL*1000UL
#define BUZZER_PERIOD_US        100UL*1000UL

#define CALA_TABLE_SIZE 16
#define CALB_TABLE_SIZE 14

#define DEFAULT_VALUE_A 50
#define DEFAULT_VALUE_B 150

#define BUZZER_MAX_RATE 10
#define BUZZER_TONE 500
#define BUZZER_DURATION_MS 10
#define BUZZER_MIN_A_DBM -25
#define BUZZER_MIN_B_DBM -35

struct cal_t {
  int v;
  int dbm;
};

cal_t calA_[CALA_TABLE_SIZE] = {
  {  0,   -91 },
  { 21,   -51 },
  { 56,   -29 },
  { 59,   -25 },
  { 60,   -21 },
  { 63,   -17 },
  { 72,   -13 },
  { 102,   -9 },
  { 122,   -5 },
  { 143,   -1 },
  { 163,    3 },
  { 204,    7 },
  { 245,   11 },
  { 409,   15 },
  { 614,   23 },
  { 1024,  33 }
};

cal_t calB_[CALB_TABLE_SIZE] = {
  {   0,  -77 },
  {  80,  -67 },
  { 100,  -57 },
  { 150,  -47 },
  { 210,  -37 },
  { 290,  -27 },
  { 308,  -17 },
  { 330,   -7 },
  { 410,    3 },
  { 480,   13 },
  { 515,   23 },
  { 540,   33 },
  { 560,   43 }
};

int valueA_, valueB_, valueA_1M_, valueB_1M_;
unsigned int buzzerRate_, buzzerCounter_;

Timer<4, micros> timer_;

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

  valueA_ = valueA_1M_ = DEFAULT_VALUE_A;
  valueB_ = valueB_1M_ = DEFAULT_VALUE_B;
  buzzerRate_ = buzzerCounter_ = 0;

  pinMode(PIN_BUZZER, OUTPUT);
  
  timer_.every(MEASURE_PERIOD_1M_US, clean1MValue);
  timer_.every(SCREEN_UPDATE_PERIOD_US, printMeasuredValue);
  timer_.every(MEASURE_PERIOD_US, measure);
  timer_.every(BUZZER_PERIOD_US, buzzer);
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
  double k = (double)(SCREEN_WIDTH) / (double)(calA_[CALA_TABLE_SIZE - 2].dbm - calA_[2].dbm);
  return k * (double)(dbm - calA_[2].dbm);
}

int toBarLengthB(int dbm) {
  double k = (double)(SCREEN_WIDTH) / (double)(calB_[CALB_TABLE_SIZE - 3].dbm - calB_[3].dbm);
  return k * (double)(dbm - calB_[3].dbm);
}

int toDbmA(int adValue) {
  int prevValue = calA_[0].v;
  int prevDbm = calA_[0].dbm;

  if (adValue <= prevValue) return prevDbm;
  
  for (int i = 1; i < CALA_TABLE_SIZE; i++) {
   if (adValue > prevValue && adValue <= calA_[i].v) {
     double k = (double)(calA_[i].dbm - prevDbm) / (double)(calA_[i].v - prevValue);
     return k * (double)(adValue - prevValue) + prevDbm;
   }
   prevValue = calA_[i].v;
   prevDbm = calA_[i].dbm;
  }
  return calA_[CALA_TABLE_SIZE - 1].dbm;
}

int toDbmB(int adValue) {
  int prevValue = calB_[0].v;
  int prevDbm = calB_[0].dbm;
  if (adValue <= prevValue) return prevDbm;
  
  for (int i = 1; i < CALB_TABLE_SIZE; i++) {
   if (adValue > prevValue && adValue <= calB_[i].v) {
     double k = (double)(calB_[i].dbm - prevDbm) / (double)(calB_[i].v - prevValue);
     return k * (adValue - prevValue) + prevDbm;
   }
   prevValue = calB_[i].v;
   prevDbm = calB_[i].dbm;
  }
  return calB_[CALB_TABLE_SIZE - 1].dbm;
}

bool clean1MValue(void *) {
  valueA_1M_ = DEFAULT_VALUE_A;
  valueB_1M_ = DEFAULT_VALUE_B;
  return true;
}

bool buzzer(void *) {
  if (buzzerRate_ > 0 && buzzerCounter_ % buzzerRate_ == 0) {
    tone(PIN_BUZZER, BUZZER_TONE, BUZZER_DURATION_MS);
  }
  else {
    noTone(PIN_BUZZER);
  }
  buzzerCounter_++;
  return true;
}

int getBuzzerRate(int dbmA, int dbmB) {
  if (dbmA > BUZZER_MIN_A_DBM && dbmA > dbmB) {
    return -0.2 * (double)(dbmA + 30) + BUZZER_MAX_RATE;
  }
  else if (dbmB > BUZZER_MIN_B_DBM) {
    return -0.14 * (double)(dbmB + 50) + BUZZER_MAX_RATE;
  }
  else {
    return 0;
  }
}

bool printMeasuredValue(void *) {
  
  display_.clearDisplay();
  display_.setTextSize(1);

  // channel A
  int dbmA = toDbmA(valueA_);
  int dbmA_1M = toDbmA(valueA_1M_);
  float mwA = toMw(dbmA);
  
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
  int dbmB = toDbmB(valueB_);
  int dbmB_1M = toDbmB(valueB_1M_);
  float mwB = toMw(dbmB);
  
  display_.drawRect(0, 2 * BAR_HEIGHT + 2, SCREEN_WIDTH, BAR_HEIGHT, SSD1306_WHITE);
  display_.fillRect(0, 2 * BAR_HEIGHT + 2, toBarLengthB(dbmB), BAR_HEIGHT, SSD1306_WHITE);

  int barB_1M = toBarLengthB(dbmB_1M);
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
  valueA_ = DEFAULT_VALUE_A; 
  valueB_ = DEFAULT_VALUE_B;

  buzzerRate_ = getBuzzerRate(dbmA, dbmB);
  return true;
}
