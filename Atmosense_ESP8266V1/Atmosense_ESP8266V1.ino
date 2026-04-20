#define BLYNK_TEMPLATE_ID   "YOUR TEMPLATE ID"
#define BLYNK_TEMPLATE_NAME "YOUR BLYNK TEMPLATE NAME"
#define BLYNK_AUTH_TOKEN    "YOUR BLYNK AUTH TOKEN"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

char ssid[] = "WIFI SSID";
char pass[] = "WIFI PASSWORD";

// Variables to store sensor data
float temperature = 0;
float pressure = 0;
int rawADC = 0;
float co2_ppm = 0;

// Optional alert threshold
const int ALARM_THRESHOLD = 400;

BlynkTimer timer;

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("NodeMCU Blynk Receiver Ready");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  timer.run();

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    if (data.length() > 0) {
      Serial.print("Received → ");
      Serial.println(data);

      // Parse "T:xx.x,P:xxxx,RAW:xxx,CO2:xxx"
      int tIndex = data.indexOf("T:");
      int pIndex = data.indexOf(",P:");
      int rIndex = data.indexOf(",RAW:");
      int cIndex = data.indexOf(",CO2:");

      if (tIndex != -1 && pIndex != -1 && rIndex != -1 && cIndex != -1) {
        temperature = data.substring(tIndex + 2, pIndex).toFloat();
        pressure    = data.substring(pIndex + 3, rIndex).toFloat();
        rawADC      = data.substring(rIndex + 5, cIndex).toInt();
        co2_ppm     = data.substring(cIndex + 5).toFloat();

        Blynk.virtualWrite(V0, temperature);
        Blynk.virtualWrite(V1, pressure);
        Blynk.virtualWrite(V2, co2_ppm);
        Blynk.virtualWrite(V3, rawADC);

        if (rawADC > ALARM_THRESHOLD) {
          Blynk.logEvent("air_quality_alert", "Poor air quality detected!");
        }

        Serial.print("Temp: "); Serial.print(temperature);
        Serial.print(" C, Press: "); Serial.print(pressure);
        Serial.print(" hPa, RAW: "); Serial.print(rawADC);
        Serial.print(", CO2: "); Serial.println(co2_ppm);
      } else {
        Serial.println("Parse error!");
      }
    }
  }
}