#include <Arduino.h>
#include <ArduinoJson.h>

#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

uint8_t grad[8] = {
  0b01000,
  0b10100,
  0b01000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

#define USE_SERIAL Serial
#define TOGGLE D6
#define ONE_WIRE_BUS D4

const long prefCheckDelay = 20000; // 20 sec
const int tempCheckDelay = 20000; // 20 sec
const int tempUploadDelay = 60000; // 60 sec

long lastPrefCheckTime = -prefCheckDelay - 1;
bool isPrefSaved = true;

long lastTempCheck = -tempCheckDelay - 1;
long lastTempUpload = -tempUploadDelay - 1;

float currentTemp;
float setupTemp = 23;

ESP8266WiFiMulti WiFiMulti;
DynamicJsonBuffer jsonBuffer;
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

void setup() {
  USE_SERIAL.begin(115200);

  WiFiMulti.addAP("SSID", "PASSWORD");

  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();
  
  lcd.clear();
  lcd.print("TempNow:");
  lcd.setCursor(0,1);
  lcd.print("Setup:");
  lcd.createChar(1, grad);

  sensors.begin();

  // setup toggle
  pinMode(TOGGLE, OUTPUT);
}

// function to print the temperature for a device
float getTemperatureC()
{
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void loop() {
  
  if(millis() - lastPrefCheckTime > prefCheckDelay && WiFiMulti.run() == WL_CONNECTED) {

    HTTPClient http;
    http.begin("https://api.thingspeak.com/channels/{channelID}/fields/1/last.json?api_key={key}",
                "78 60 18 44 81 35 bf df 77 84 d4 0a 22 0d 9b 4e 6c dc 57 2c"); //HTTP
                
    int httpCode = http.GET();

    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      JsonObject& root = jsonBuffer.parseObject(payload);
      setupTemp = root["field1"];
      
      USE_SERIAL.print("Setup Temp: ");
      USE_SERIAL.println(setupTemp);
    } else {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    lastPrefCheckTime = millis();
  }

  if (millis() - lastTempCheck > tempCheckDelay) {
  
    lcd.setCursor(8,0);
    currentTemp = getTemperatureC();
    lcd.print(currentTemp);
    lcd.print(" C");  
    lcd.write(1);
    
    lcd.setCursor(8,1);
    lcd.print(setupTemp);
    lcd.print(" C");
    lcd.write(1);

    Serial.print("Temp C: ");
    Serial.println(currentTemp);
    
    if (currentTemp < setupTemp) {
      digitalWrite(TOGGLE, HIGH);
    } else {
      digitalWrite(TOGGLE, LOW);
    }

    lastTempCheck = millis();

    if (WiFiMulti.run() == WL_CONNECTED && millis() - lastTempUpload > tempUploadDelay)
    {
      HTTPClient http;

      char url[90] = "https://api.thingspeak.com/update?api_key={key}&field2=";
      
      char buffer[16];
      dtostrf(currentTemp, 5, 2,buffer);
      strcat(url, buffer);

      http.begin(url, 
          "78 60 18 44 81 35 bf df 77 84 d4 0a 22 0d 9b 4e 6c dc 57 2c");
                  
      int httpCode = http.GET();
      http.getString();
      if (httpCode != HTTP_CODE_OK)
      {
        USE_SERIAL.printf("[HTTP] Posting results failed, error: %s\n", http.errorToString(httpCode).c_str());
      } 
      else 
      {
        lastTempUpload = millis();  
      }
      http.end();
    }
  }
}

