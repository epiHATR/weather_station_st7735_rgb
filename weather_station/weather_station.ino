#include <WiFi.h>

#include <Adafruit_GFX.h>     // include Adafruit graphics library
#include <Adafruit_ST7735.h>  // include Adafruit ST7735 TFT library
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include "sntp.h"

#define ST7735_GRID_BG ST7735_BLACK  //0xFFFF //ST7735_BLACK
#define TFT_CS 3                     // TFT CS  pin is connected to XIAO ESP32C3 pin D1 (GPIO3)
#define TFT_DC 4                     // TFT DC  pin is connected to XIAO ESP32C3 pin D2 (GPIO4)
#define TFT_RST 5                    // TFT RST pin is connected to XIAO ESP32C3 pin D3 (GPIO5)
// initialize ST7735 TFT library with hardware SPI module
// SCK (CLK) ---> XIAO ESP32C3 pin D8 (GPIO8)
// MOSI(DIN) ---> XIAO ESP32C3 pin D10 (GPIO10)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const int BUTTON_NEXT_PIN = 6;
const int BUTTON_SEL_PIN = 7;
const int BUTTON_PREV_PIN = 9;

//CHANGE THIS VALUE MATCHES WITH YOUR CONFIGURATIONS
const char* ssid = "<YOUR WIFI SSID>";
const char* password = "YOUR WIFI PASSWORD";
const String weather_apiKey = "YOUR WEATHER API KEY";
const String cityCode = "YOUR CITY CODE";
//CHANGE THIS VALUE MATCHES WITH YOUR CONFIGURATIONS

#define LED_PIN 21        // control the blinking LED
#define BACKLIGHT_PIN 20  // Control ON/OFF back light on the screen

bool nextButtonReleased = true;
bool prevButtonReleased = true;
bool selButtonReleased = true;

long screenTimeOutInSecond = 30;
long screenStartTime = 0;
long screenState = HIGH;
const char* temp_ssid = "connecting";
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char* time_zone = "ICT-7,M3.5.5/0,M10.5.5/0";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

int offset_left = 2;
int offset_top = 1;

enum Pages {
  home,
  stopwatch,
  settings
};

Pages page = home;

// for controling weather widget
float batteryLevel = 0;
long weatherFetchTimeoutInSecond = 30;
long batteryCheckTimeoutInSecond = 5;
long hourCheckTimeoutInSecond = 30;

long lastCheckConnection = 0;
long lastCheckWeather = 0;
long lastCheckTime = 0;
long lastCheckBattery = 0;
long lastCheckIndicator = 0;
long indicator_position = 0;

//STOPWATCH
long lastLoadSTW = 0;
long lastLoadMls = 0;
long lastLoadSec = 0;
long lastLoadMin = 0;
long lastLoadHrs = 0;

unsigned long ms;
unsigned long startMS;
unsigned long currentMS;
unsigned long elapsedMS;

long runningSEC = 0;
long runningMIN = 0;
long runningHRS = 0;

bool isStopwatchRunning = false;
unsigned long durMLS;
unsigned long durSEC;
unsigned long durMIN;
unsigned long durHRS;

long cellWidth = 40;
long cellHeight = 25;
long spliterWith = 15;
long spacing = 1;
long marginTop = offset_top + 36;

long ONduration = 80;
long OFFduration = 1000;
long LED_STATE = LOW;
long rememberTime = 0;

int incomingByte = 0;

//SETTINGS
long lastLoadSTS = 0;

long currentScreen = 0;
bool isScreenClear = false;

float maxBat = 4.2;
float minBat = 3.7;

String jsonBuffer;

void Print_LocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }

  // print week day
  char weekDay[10];
  strftime(weekDay, sizeof(weekDay), "%a", &timeinfo);

  char dayOfMonth[3];
  strftime(dayOfMonth, sizeof(dayOfMonth), "%d", &timeinfo);

  char monthOfyear[10];
  strftime(monthOfyear, sizeof(monthOfyear), "%b", &timeinfo);

  char year[10];
  strftime(year, sizeof(year), "%Y", &timeinfo);

  tft.setTextSize(1);
  tft.fillRect(offset_left, offset_top + 25, 95, 8, ST7735_GRID_BG);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(offset_left, offset_top + 25);
  tft.printf("%s %s %s %s", weekDay, dayOfMonth, monthOfyear, year);

  // print Hour:Min
  GFXcanvas1 canvas(95, 21);
  canvas.setTextSize(3);
  canvas.setTextWrap(true);
  char timeHour[3];
  char timeMin[3];
  strftime(timeHour, 3, "%H", &timeinfo);
  strftime(timeMin, 3, "%M", &timeinfo);
  canvas.print(timeHour);
  canvas.print(":");
  canvas.println(timeMin);
  tft.drawBitmap(offset_left, offset_top + 40, canvas.getBuffer(), 95, 21, ST7735_ORANGE, ST7735_GRID_BG);
}

// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  Print_LocalTime();
}

void Print_Temperature(String tempC, String tempF, String cityName) {
  GFXcanvas1 canvas_tempc(32, 19);
  canvas_tempc.setTextSize(2);
  canvas_tempc.print(tempC);
  canvas_tempc.setTextWrap(true);
  tft.drawBitmap(offset_left + 100, offset_top + 25, canvas_tempc.getBuffer(), 32, 19, ST7735_CYAN, ST7735_GRID_BG);

  GFXcanvas1 canvas_symbol(10, 19);
  canvas_symbol.setTextSize(1);
  canvas_symbol.print("o");
  canvas_symbol.setTextWrap(true);
  tft.drawBitmap(offset_left + 133, offset_top + 25, canvas_symbol.getBuffer(), 10, 19, ST7735_CYAN, ST7735_GRID_BG);

  GFXcanvas1 canvas_tempf(15, 19);
  canvas_tempf.setTextSize(2);
  canvas_tempf.print("C");
  canvas_tempf.setTextWrap(true);
  tft.drawBitmap(offset_left + 144, offset_top + 25, canvas_tempf.getBuffer(), 15, 19, ST7735_CYAN, ST7735_GRID_BG);


  GFXcanvas1 canvas_tempcity(60, 10);
  canvas_tempcity.setTextSize(1);
  canvas_tempcity.print(cityName);
  canvas_tempcity.setTextWrap(true);
  tft.drawBitmap(offset_left + 100, offset_top + 45, canvas_tempcity.getBuffer(), 60, 10, ST7735_CYAN, ST7735_GRID_BG);
  tft.setFont();
}

void Setup_Network_and_Time() {
  // set notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);
  //sntp_servermode_dhcp(1);  // (optional)

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  configTzTime(time_zone, ntpServer1, ntpServer2);

  //connect to WiFi
  Serial.printf("connecting to WIFI %s \n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to WIFI");
}

void Print_SignalBars() {
  tft.fillRect(offset_left + 1, offset_top + 6, 2, 2, ST7735_GREEN);
  tft.fillRect(offset_left + 4, offset_top + 4, 2, 4, ST7735_GREEN);
  tft.fillRect(offset_left + 7, offset_top + 0, 2, 8, ST7735_GREEN);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(15, offset_top);
  tft.fillRect(offset_left + 15, offset_top + 0, 80, 9, ST7735_GRID_BG);
  tft.print(temp_ssid);
}

void Print_BatteryLevel(int percent) {
  int offset = 140;
  int width = 12;

  u_int BAT_COLOR = ST7735_WHITE;
  if (percent > 100) {
    percent = 100;
    u_int BAT_COLOR = ST7735_GREEN;
  } else {
    tft.fillRect(offset_left + offset - 25, offset_top, tft.width(), 8, ST7735_BLACK);
  }

  tft.setTextSize(1);
  if (percent > 75) {
    BAT_COLOR = ST7735_GREEN;
    tft.setCursor(offset - 25, offset_top);
  } else if (percent >= 10 && percent <= 75) {
    BAT_COLOR = ST7735_YELLOW;
    tft.setCursor(offset - 17, offset_top);
  } else {
    BAT_COLOR = ST7735_ORANGE;
    tft.setCursor(offset - 10, offset_top);
  }

  if (percent <= 10) {
    width = width - 10;
  }
  if (percent > 10 && percent <= 75) {
    width = width - 5;
  } else {
    width = width;
  }
  if (percent <= 100) {
    tft.fillRect(offset_left + offset + 3, offset_top, width, 8, BAT_COLOR);
    tft.fillRect(offset_left + offset + 1, offset_top + 2, 2, 4, BAT_COLOR);
    tft.setTextColor(BAT_COLOR);
    tft.print(String(percent) + "%");
  }
}

String httpGETRequest(const char* serverName) {
  HTTPClient http;
  // Your IP address with path or Domain name with URL path
  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void Print_WeatherInfo() {
  Serial.println("calling to the weather API");
  //call API to weather here
  String weatherAPI = "https://api.weatherapi.com/v1/current.json?key=" + weather_apiKey + "&q=" + cityCode + "&aqi=yes";

  jsonBuffer = httpGETRequest(weatherAPI.c_str());
  JSONVar myObject = JSON.parse(jsonBuffer);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  String cityName = myObject["location"]["name"];
  int tempC = myObject["current"]["temp_c"];
  int tempF = myObject["current"]["temp_f"];
  Print_Temperature(String(tempC), String(tempF), cityName);

  String condition = myObject["current"]["condition"]["text"];
  tft.fillRect(offset_left, offset_top + 66, 160, 10, ST7735_GRID_BG);
  tft.setCursor(offset_left, offset_top + 66);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);

  if (WiFi.status() == WL_CONNECTED) {
    tft.print(condition);
  } else {
    temp_ssid = ssid;
    tft.printf("fetching weather info");
  }
}

void Print_Indicator() {
  if ((millis() - lastCheckIndicator >= 3)) {
    indicator_position++;
    lastCheckIndicator = millis();
  }

  if (indicator_position <= 160) {
    tft.fillRect(offset_left + indicator_position, offset_top + 78, 1, 1, ST7735_ORANGE);
  } else {
    tft.fillRect(offset_left, offset_top + 78, 160, 1, ST7735_BLACK);
    indicator_position = 0;
  }
}

void Print_HomeScreen() {
  if (isScreenClear == false) {
    tft.fillScreen(ST7735_BLACK);
    isScreenClear = true;
  }

  if (millis() - lastCheckConnection >= 2000) {
    Print_SignalBars();
    lastCheckConnection = millis();
  }

  if (millis() - lastCheckBattery >= batteryCheckTimeoutInSecond * 1000) {
    lastCheckBattery = millis();
    uint32_t Vbatt = 0;
    for (int i = 0; i < 16; i++) {
      Vbatt = Vbatt + analogReadMilliVolts(A0);
    }

    float Vbattf = 2 * Vbatt / 16 / 1000.0;

    if (Vbattf >= minBat) {
      batteryLevel = ((Vbattf - minBat) / (maxBat - minBat)) * 100;
      Serial.println(batteryLevel);
    } else {
      batteryLevel = 0;
    }
    Print_BatteryLevel(batteryLevel);
  }

  // render DATE & TIME
  if ((millis() - lastCheckTime >= hourCheckTimeoutInSecond * 1000) || (lastCheckTime == 0)) {
    Print_LocalTime();
    lastCheckTime = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    lastCheckWeather = 0;
  } else {
    temp_ssid = ssid;
  }

  // render WEATHER & LOCATION
  if ((millis() - lastCheckWeather >= weatherFetchTimeoutInSecond * 1000 || lastCheckWeather == 0) && WiFi.status() == WL_CONNECTED) {
    Print_WeatherInfo();
    lastCheckWeather = millis();
  }

  // print indicator underneath the screen
  //Print_Indicator();
}

void Print_SettingPage() {
  if (isScreenClear == false) {
    tft.fillScreen(ST7735_BLACK);
    isScreenClear = true;
  }

  if (millis() - lastCheckConnection >= 2000 || lastCheckConnection == 0) {
    Print_SignalBars();
    lastCheckConnection = millis();
  }

  if (millis() - lastCheckBattery >= batteryCheckTimeoutInSecond * 1000 || lastCheckBattery == 0) {
    batteryLevel++;
    lastCheckBattery = millis();
    Print_BatteryLevel(batteryLevel);
  }

  if (millis() - lastLoadSTS >= 10000 || lastLoadSTS == 0) {
    lastLoadSTS = millis();
    tft.setTextSize(1);
    tft.fillRect(offset_left, offset_top + 25, 95, 8, ST7735_GRID_BG);
    tft.setTextColor(ST7735_CYAN);
    tft.setCursor(offset_left, offset_top + 25);
    tft.printf("SETTINGS");
  }
}

void Print_StopWatchPage() {
  if (isScreenClear == false) {
    tft.fillScreen(ST7735_BLACK);
    isScreenClear = true;
  }

  if (millis() - lastCheckConnection >= 2000 || lastCheckConnection == 0) {
    Print_SignalBars();
    lastCheckConnection = millis();
  }

  if (millis() - lastCheckBattery >= batteryCheckTimeoutInSecond * 1000 || lastCheckBattery == 0) {
    batteryLevel++;
    lastCheckBattery = millis();
    Print_BatteryLevel(batteryLevel);
  }

  if (millis() - lastLoadSTW >= 10000 || lastLoadSTW == 0) {
    lastLoadSTW = millis();
    tft.setTextSize(1);
    tft.fillRect(offset_left, offset_top + 25, 95, 8, ST7735_GRID_BG);
    tft.setTextColor(ST7735_CYAN);
    tft.setCursor(offset_left, offset_top + 25);
    tft.print("STOPWATCH");
  }

  if (isStopwatchRunning) {
    ms = millis();
    elapsedMS = (ms - startMS);

    durMLS = elapsedMS % 1000;
    durSEC = (elapsedMS / 1000) % 60;
    durMIN = (elapsedMS / (60000)) % 60;
    durHRS = (elapsedMS / (3600000));
  }

  // print HOURRS
  if (millis() - lastLoadHrs >= 1000) {
    lastLoadHrs = millis();
    tft.setTextSize(3);
    tft.fillRect(offset_left, marginTop, cellWidth, cellHeight, ST7735_GRID_BG);
    tft.setTextColor(ST7735_ORANGE);
    tft.setCursor(offset_left + 4, marginTop);
    if (durHRS < 10) {
      tft.print("0");
    }
    tft.print(durHRS);
  }
  // print SPLITER
  tft.setTextSize(3);
  tft.fillRect(offset_left + cellWidth + spacing, marginTop, spliterWith, cellHeight, ST7735_GRID_BG);
  tft.setTextColor(ST7735_ORANGE);
  tft.setCursor(offset_left + cellWidth + spacing, marginTop);
  tft.print(":");

  // print MINUTE
  if (millis() - lastLoadMin >= 1000) {
    lastLoadMin = millis();
    tft.setTextSize(3);
    tft.fillRect(offset_left + cellWidth + spacing + spliterWith + spacing, marginTop, cellWidth, cellHeight, ST7735_GRID_BG);
    tft.setTextColor(ST7735_ORANGE);
    tft.setCursor(offset_left + cellWidth + spacing + spliterWith + spacing + 4, marginTop);
    if (durMIN < 10) {
      tft.print("0");
    }
    tft.print(durMIN);
  }

  // print SPLITER
  tft.setTextSize(3);
  tft.fillRect(offset_left + cellWidth + spacing + spliterWith + spacing + cellWidth + spacing, marginTop, spliterWith, cellHeight, ST7735_GRID_BG);
  tft.setTextColor(ST7735_ORANGE);
  tft.setCursor(offset_left + cellWidth + spacing + spliterWith + spacing + cellWidth + spacing, marginTop);
  tft.print(":");

  // print SECOND
  if (millis() - lastLoadSec >= 1000) {
    lastLoadSec = millis();
    tft.setTextSize(3);
    tft.fillRect(offset_left + cellWidth + spacing + spliterWith + spacing + cellWidth + spacing + spliterWith + spacing, marginTop, cellWidth, cellHeight, ST7735_GRID_BG);
    tft.setTextColor(ST7735_ORANGE);
    tft.setCursor(offset_left + cellWidth + spacing + spliterWith + spacing + cellWidth + spacing + spliterWith + spacing + 4, marginTop);
    if (durSEC < 10) {
      tft.print("0");
    }
    tft.print(durSEC);
  }

  // print MILLISECOND
  if (millis() - lastLoadMls >= 100) {
    lastLoadMls = millis();
    tft.setTextSize(1);
    tft.fillRect(offset_left, marginTop + 27, 160, 20, ST7735_GRID_BG);
    tft.setTextColor(ST7735_ORANGE);
    tft.setCursor(offset_left + 70, marginTop + 29);
    if (durMLS < 10) {
      tft.print("000");
    } else if (durMLS < 100) {
      tft.print("00");
    } else if (durMLS < 1000) {
      tft.print("0");
    }
    tft.print(durMLS);
  }
}

void ResetAllCheck() {
  lastCheckConnection = 0;
  lastCheckBattery = 0;
  lastCheckWeather = 0;
  lastCheckTime = 0;
  lastLoadSTS = 0;
  lastLoadSTW = 0;
}

void TurnOn_Screen() {
  screenState = HIGH;
  screenStartTime = millis();
  digitalWrite(BACKLIGHT_PIN, screenState);
}

void Handle_Buttons() {
  switch (page) {
    case home:
      {
        if (digitalRead(BUTTON_NEXT_PIN) == LOW && nextButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          nextButtonReleased = false;
          page = stopwatch;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_NEXT_PIN) == HIGH) {
          nextButtonReleased = true;
        }

        if (digitalRead(BUTTON_PREV_PIN) == LOW && prevButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          prevButtonReleased = false;
          page = settings;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_PREV_PIN) == HIGH) {
          prevButtonReleased = true;
        }
      }
      break;
    case stopwatch:
      {
        if (digitalRead(BUTTON_NEXT_PIN) == LOW && nextButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          nextButtonReleased = false;
          page = settings;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_NEXT_PIN) == HIGH) {
          nextButtonReleased = true;
        }

        if (digitalRead(BUTTON_PREV_PIN) == LOW && prevButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          prevButtonReleased = false;
          page = home;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_PREV_PIN) == HIGH) {
          prevButtonReleased = true;
        }
      }
      //handle SELECT button
      if (digitalRead(BUTTON_SEL_PIN) == LOW && selButtonReleased == true) {
        selButtonReleased = false;
        if (!isStopwatchRunning) {
          isStopwatchRunning = true;
          startMS = millis();
          elapsedMS = 0;
          runningSEC = 0;
          runningMIN = 0;
          runningHRS = 0;
        } else {
          isStopwatchRunning = false;
        }
        TurnOn_Screen();
      } else if (digitalRead(BUTTON_SEL_PIN) == HIGH) {
        selButtonReleased = true;
      }
      break;

    case settings:
      {
        //handle NEXT button
        if (digitalRead(BUTTON_NEXT_PIN) == LOW && nextButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          nextButtonReleased = false;
          page = home;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_NEXT_PIN) == HIGH) {
          nextButtonReleased = true;
        }

        //handle NEXT button
        if (digitalRead(BUTTON_PREV_PIN) == LOW && prevButtonReleased == true) {
          ResetAllCheck();
          isScreenClear = false;
          prevButtonReleased = false;
          page = stopwatch;
          TurnOn_Screen();
        } else if (digitalRead(BUTTON_PREV_PIN) == HIGH) {
          prevButtonReleased = true;
        }
      }
      break;
    default:
      {
        isScreenClear = false;
        nextButtonReleased = true;
        page = home;
        TurnOn_Screen();
      }
      break;
  }
}

void Blinking_LED() {
  if (LED_STATE == HIGH) {
    if ((millis() - rememberTime) >= ONduration) {
      LED_STATE = LOW;
      rememberTime = millis();
    }
  } else {
    if ((millis() - rememberTime) >= OFFduration) {
      LED_STATE = HIGH;
      rememberTime = millis();
    }
  }

  digitalWrite(LED_PIN, LED_STATE);
}

void Blinking_SCREEN() {
  if (screenState == HIGH) {
    if ((millis() - screenStartTime) >= screenTimeOutInSecond * 1000) {
      Serial.println("SCREEN is being OFF");
      screenState = LOW;
      screenStartTime = millis();
      digitalWrite(BACKLIGHT_PIN, screenState);
    }
  } else {
    if ((millis() - screenStartTime) >= 3 * 1000) {
      Serial.println("SCREEN is being ON");
      screenState = HIGH;
      screenStartTime = millis();
      digitalWrite(BACKLIGHT_PIN, screenState);
    }
  }
}

void Handle_SerialInput() {
  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();
    if (incomingByte) {
      screenState = HIGH;
      screenStartTime = millis();
      digitalWrite(BACKLIGHT_PIN, screenState);
    }
  }
}

void setup(void) {
  Serial.begin(9600);

  pinMode(A0, INPUT);

  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, screenState);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_STATE);
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SEL_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PREV_PIN, INPUT_PULLUP);

  tft.initR(INITR_MINI160x80_PLUGIN);
  tft.setRotation(3);
  tft.fillScreen(ST7735_BLACK);
  page = home;
  Setup_Network_and_Time();
}

void loop() {
  Handle_Buttons();
  Blinking_LED();
  Blinking_SCREEN();
  Handle_SerialInput();
  switch (page) {
    case home: Print_HomeScreen(); break;
    case stopwatch: Print_StopWatchPage(); break;
    case settings: Print_SettingPage(); break;
    default: Print_HomeScreen(); break;
  }
}