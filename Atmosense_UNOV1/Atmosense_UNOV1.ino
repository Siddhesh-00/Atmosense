#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_BMP280.h>
#include <SoftwareSerial.h>

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_BMP280 bmp;
SoftwareSerial espSerial(2, 3);  // RX, TX

// Pins
#define MQ135_PIN   A0
#define BUZZER_PIN  7
#define GREEN_LED   8
#define RED_LED     9

// Settings
const int ALARM_THRESHOLD = 400;
const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long DISPLAY_INTERVAL = 40;
const unsigned long BEEP_INTERVAL = 250;

#define BUZZER_FREQ 2000

// Sensor data
float co2_ppm = 0, co2_smooth = 0;
int rawADC = 0;
bool alarmActive = false;

float temp = 0, temp_smooth = 0;
float pressure = 0, pres_smooth = 0;
bool bmp_ok = false;

unsigned long lastSensorRead = 0, lastBMPRead = 0, lastBeep = 0, lastDisplay = 0;
bool beepState = false;

// ------------------------------------------------------------
void buzzerOn()  { tone(BUZZER_PIN, BUZZER_FREQ); }
void buzzerOff() { noTone(BUZZER_PIN); }

void i2cReset() {
  Wire.end(); delay(20);
  Wire.begin(); Wire.setClock(100000);
}

// --- Icons (same as before) ---
void drawLeafIcon(int x, int y) {
  u8g2.drawCircle(x+5, y+5, 4);
  u8g2.drawLine(x+2, y+8, x+8, y+2);
}

void drawThermometerIcon(int x, int y) {
  u8g2.drawFrame(x+2, y, 3, 7);
  u8g2.drawCircle(x+3, y+8, 2);
  u8g2.drawBox(x+3, y+2, 1, 4);
}

void drawPressureIcon(int x, int y) {
  u8g2.drawCircle(x+4, y+5, 4);
  u8g2.drawLine(x+4, y+5, x+7, y+2);
  u8g2.drawLine(x+4, y+1, x+4, y+3);
}

void drawPulseBars(int x, int y, unsigned long now) {
  for (int i=0; i<4; i++) {
    int h = 2 + ((now/100 + i*3) % 6);
    u8g2.drawVLine(x + i*3, y + (7-h), h);
  }
}

void drawRawIcon(int x, int y) {
  u8g2.drawVLine(x+1, y+6, 4);
  u8g2.drawVLine(x+3, y+4, 6);
  u8g2.drawVLine(x+5, y+2, 8);
}

void drawThIcon(int x, int y) {
  u8g2.drawLine(x+2, y+1, x+2, y+9);
  u8g2.drawLine(x+2, y+1, x+8, y+3);
  u8g2.drawLine(x+2, y+5, x+8, y+7);
}

// --- Main Dashboard ---
void drawMainScreen() {
  unsigned long now = millis();

  co2_smooth += (co2_ppm - co2_smooth) * 0.15;
  temp_smooth += (temp - temp_smooth) * 0.1;
  pres_smooth += (pressure - pres_smooth) * 0.1;

  u8g2.firstPage();
  do {
    // Header
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(3, 9, "Atmosense");
    u8g2.setFont(u8g2_font_5x7_tf);
    if (alarmActive) u8g2.drawStr(100, 9, "ALARM");
    else u8g2.drawStr(95, 9, "Normal");

    u8g2.drawHLine(0, 12, 128);

    // Air Quality
    u8g2.drawRFrame(2, 14, 124, 20, 3);
    drawLeafIcon(3, 19);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(15, 27, "AIR QUALITY");
    u8g2.setFont(u8g2_font_9x15B_tf);
    u8g2.setCursor(78, 29);
    u8g2.print(co2_smooth, 0);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(105, 30, "ppm");
    drawPulseBars(108, 16, now);

    if (alarmActive && ((now/180)%2 == 0))
      u8g2.drawFrame(0, 0, 128, 64);

    // Temperature
    u8g2.drawRFrame(2, 36, 60, 14, 2);
    drawThermometerIcon(4, 37);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(12, 46, "T:");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(24, 46);
    if (bmp_ok) {
      u8g2.print(temp_smooth,1);
      u8g2.print("C");
    } else {
      u8g2.print("--.-C");
    }

    // Pressure
    u8g2.drawRFrame(64, 36, 62, 14, 2);
    drawPressureIcon(66, 37);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(76, 46, "P:");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(86, 46);
    if (bmp_ok) {
      u8g2.print(pres_smooth,0);
      u8g2.print("hPa");
    } else {
      u8g2.print("----hPa");
    }

    // Footer
    u8g2.drawRFrame(2, 52, 58, 11, 2);
    drawRawIcon(4, 51);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(14, 60);
    u8g2.print("RAW:");
    u8g2.setCursor(36, 60);
    u8g2.print(rawADC);

    u8g2.drawRFrame(64, 52, 62, 11, 2);
    drawThIcon(66, 52);
    u8g2.setCursor(76, 60);
    u8g2.print("TH:");
    u8g2.setCursor(94, 60);
    u8g2.print(ALARM_THRESHOLD);

  } while (u8g2.nextPage());
}

// ------------------------------------------------------------
void setup() {
  Serial.begin(9600);           // USB debug
  espSerial.begin(9600);        // Communication with NodeMCU

  Wire.begin();
  Wire.setClock(100000);

  u8g2.setI2CAddress(0x3C << 1);
  u8g2.begin();

  pinMode(MQ135_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  buzzerOff();
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Startup splash (simplified)
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_9x15B_tf);
    u8g2.drawStr(20, 30, "Atmosense");
  } while (u8g2.nextPage());
  delay(1000);

  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    bmp_ok = true;
    temp = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0f;
    temp_smooth = temp;
    pres_smooth = pressure;
  }

  buzzerOn(); delay(80); buzzerOff();
  delay(60);
  buzzerOn(); delay(80); buzzerOff();
}

// ------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // --- Read BMP280 ---
  if (bmp_ok && now - lastBMPRead >= SENSOR_INTERVAL) {
    lastBMPRead = now;
    i2cReset();
    float t = bmp.readTemperature();
    float p = bmp.readPressure() / 100.0f;
    if (t > -40 && t < 85 && p > 300 && p < 1100) {
      temp = t;
      pressure = p;
    } else {
      bmp_ok = false;
    }
  }

  // --- Read MQ135 and send data ---
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    rawADC = analogRead(MQ135_PIN);
    co2_ppm = (rawADC * 5.0f / 1023.0f) * 100.0f;
    alarmActive = (rawADC > ALARM_THRESHOLD);

    // 📤 Send to NodeMCU
    espSerial.print("T:");
    espSerial.print(temp, 1);
    espSerial.print(",P:");
    espSerial.print(pressure, 0);
    espSerial.print(",RAW:");
    espSerial.print(rawADC);
    espSerial.print(",CO2:");
    espSerial.println(co2_ppm, 0);

    // Optional USB debug
    Serial.print("Sent → T:");
    Serial.print(temp, 1);
    Serial.print(" P:");
    Serial.print(pressure, 0);
    Serial.print(" RAW:");
    Serial.print(rawADC);
    Serial.print(" CO2:");
    Serial.println(co2_ppm, 0);
  }

  // --- Buzzer ---
  if (alarmActive) {
    if (now - lastBeep >= BEEP_INTERVAL) {
      lastBeep = now;
      beepState = !beepState;
      beepState ? buzzerOn() : buzzerOff();
    }
  } else {
    beepState = false;
    buzzerOff();
  }

  // --- LEDs ---
  digitalWrite(RED_LED, alarmActive ? HIGH : LOW);
  digitalWrite(GREEN_LED, !alarmActive);

  // --- OLED refresh ---
  if (now - lastDisplay >= DISPLAY_INTERVAL) {
    lastDisplay = now;
    drawMainScreen();
  }

  delay(10);
}