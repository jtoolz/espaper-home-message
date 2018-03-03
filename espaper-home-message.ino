#include <JsonStreamingParser.h>
#include <JsonListener.h>

/**The MIT License (MIT)
Copyright (c) 2017 by Daniel Eichhorn
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
See more at https://blog.squix.org
  Copyright (c) 2017 by Daniel Eichhorn
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
*/


/*****************************
   Important: see settings.h to configure your settings!!!
 * ***************************/
#include "settings.h"

#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include "MessageFonts.h"

/***
   Install the following libraries through Arduino Library Manager
   - Mini Grafx by Daniel Eichhorn
   - ESP8266 WeatherStation by Daniel Eichhorn
   - Json Streaming Parser by Daniel Eichhorn
   - simpleDSTadjust by neptune2
 ***/

#include <JsonListener.h>
#include <MiniGrafx.h>
#include <EPD_WaveShare.h>

#include "ArialRounded.h"
#include <MiniGrafxFonts.h>
#include "weathericons.h"
#include "configportal.h"
#include "HomeMessage.h"

#define MINI_BLACK 0
#define MINI_WHITE 1

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                     };

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 296
#define BITS_PER_PIXEL 1


EPD_WaveShare epd(EPD2_9, CS, RST, DC, BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

HomeMsgFields message;

String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawButtons();
void drawLabelValue(uint8_t line, String label, String value);
void drawBattery();
String getMeteoconIcon(String iconText);
const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);
void drawForecast();


/*
 GET /_ah/api/home_message/v1/messages HTTP/1.1
Host: twilio-home-msg-api.appspot.com
Cache-Control: no-cache
Postman-Token: 76ffdd4a-c1f3-4c10-a485-848b304517c2
 */
long lastDownloadUpdate = millis();

String moonAgeImage = "";
uint16_t screen = 0;
long timerPress;
bool canBtnPress;

boolean connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  //Manual Wifi
  Serial.print("[");
  Serial.print(WIFI_SSID.c_str());
  Serial.print("]");
  Serial.print("[");
  Serial.print(WIFI_PASS.c_str());
  Serial.print("]");
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    i++;
    if (i > 20) {
      Serial.println("Could not connect to WiFi");
      return false;
    }
    Serial.print(".");
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(USR_BTN, INPUT_PULLUP);
  int btnState = digitalRead(USR_BTN);

  gfx.init();
  gfx.setRotation(1);
  gfx.setFastRefresh(false);
  
  // load config if it exists. Otherwise use defaults.
  boolean mounted = SPIFFS.begin();
  if (!mounted) {
    SPIFFS.format();
    SPIFFS.begin();
  }
  loadConfig();

  Serial.println("State: " + String(btnState));
  if (btnState == LOW) {
    boolean connected = connectWifi();
    startConfigPortal(&gfx);
  } else {
    boolean connected = connectWifi();
    if (connected) {
      updateData();
      gfx.fillBuffer(MINI_WHITE);

      drawTime();
      drawBattery();
      drawMessage();
      drawButtons();
      gfx.commit();
    } else {
      gfx.fillBuffer(MINI_WHITE);
      gfx.setColor(MINI_BLACK);
      gfx.setTextAlignment(TEXT_ALIGN_CENTER);
      gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, "Could not connect to WiFi\nPress LEFT + RIGHT button\nto enter config mode");
      gfx.commit();
    }

    ESP.deepSleep(UPDATE_INTERVAL_SECS * 1000000);
  }
}


void loop() {


}

// Update the internet based information and update screen
void updateData() {
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);

  //gfx.fillBuffer(MINI_WHITE);
  gfx.setColor(MINI_BLACK);
  gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
  gfx.setColor(MINI_WHITE);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12, "Refreshing data...");
  gfx.commit();

  //gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);

  HomeMessage *homeMsgClient = new HomeMessage();
    Serial.println("Created HomeMessage, updating...");
  homeMsgClient->updateMessage(&message);
  delete homeMsgClient;
  homeMsgClient = nullptr;

  // Wait max. 3 seconds to make sure the time has been sync'ed
  Serial.println("\nWaiting for time");
  unsigned timeout = 3000;
  unsigned start = millis();
  while (millis() - start < timeout) {
    time_t now = time(nullptr);
    if (now > (2016 - 1970) * 365 * 24 * 3600) {
      return;
    }
    Serial.println(".");
    delay(100);
  }

}

// draws the clock
void drawTime() {

  char *dstAbbrev;
  char time_str[30];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(MINI_BLACK);
  String date = ctime(&now);
  date = date.substring(0, 11) + String(1900 + timeinfo->tm_year);

  if (IS_STYLE_12HR) {
    int hour = (timeinfo->tm_hour + 11) % 12 + 1; // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d", hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(2, -2, String(FPSTR(TEXT_UPDATED)) + String(time_str));
  } else {
    sprintf(time_str, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(2, -2, String(FPSTR(TEXT_UPDATED)) + String(time_str));
  }
  gfx.drawLine(0, 11, SCREEN_WIDTH, 11);

}

// draws current weather information
void drawMessage() {
  String str_year, str_month, str_day;

  // gfx.setColor(MINI_BLACK);
  //gfx.drawXbm(1, 16, 50, 50, RoundBox_bits);
  //gfx.setColor(MINI_WHITE);
//  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
//  gfx.setFont(Meteocons_Plain_42);
//  String weatherIcon = getMeteoconIcon(conditions.weatherIcon);
//  gfx.drawString(5, 20, weatherIcon);
  // Weather Text

  // Draw sender name
  gfx.setColor(MINI_BLACK);
  gfx.setFont(ArialMT_Plain_16);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5, 15, "From: " + message.senderName);

  // Draw message text
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  //gfx.drawString(5, 25, message.content);
  gfx.drawStringMaxWidth(5, 35, 285, message.content);

  // Draw date/time created
  gfx.setFont(ArialMT_Plain_16);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);

  
  str_year = message.created.substring(0,4);
  str_month = message.created.substring(5,7);
  str_day = message.created.substring(8,10);
  gfx.drawString(5, 95, months[str_month.toInt()-1] + " " + str_day + ", " + str_year);
  // gfx.drawLine(0, 65, SCREEN_WIDTH, 65);
}


void drawLabelValue(uint8_t line, String label, String value) {
  const uint8_t labelX = 15;
  const uint8_t valueX = 150;
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(labelX, 30 + line * 15, label);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(valueX, 30 + line * 15, value);
}



void drawBattery() {
  uint8_t percentage = 100;
  float adcVoltage = analogRead(A0) / 1024.0;
  // This values where empirically collected
  float batteryVoltage = adcVoltage * 4.945945946 -0.3957657658;
  if (batteryVoltage > 4.2) percentage = 100;
  else if (batteryVoltage < 3.3) percentage = 0;
  else percentage = (batteryVoltage - 3.3) * 100 / (4.2 - 3.3);

  gfx.setColor(MINI_BLACK);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH - 22, -1, String(batteryVoltage, 2) + "V " + String(percentage) + "%");
  gfx.drawRect(SCREEN_WIDTH - 22, 0, 19, 10);
  gfx.fillRect(SCREEN_WIDTH - 2, 2, 2, 6);
  gfx.fillRect(SCREEN_WIDTH - 20, 2, 16 * percentage / 100, 6);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  int8_t quality = getWifiQuality();
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(228, 9, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 18 - j);
      }
    }
  }
}


void drawButtons() {
  gfx.setColor(MINI_BLACK);

  uint16_t third = SCREEN_WIDTH / 3;
  gfx.setColor(MINI_BLACK);
  //gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
  gfx.drawLine(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, SCREEN_HEIGHT - 12);
  gfx.drawLine(2 * third, SCREEN_HEIGHT - 12, 2 * third, SCREEN_HEIGHT);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(0.5 * third, SCREEN_HEIGHT - 12, FPSTR(TEXT_CONFIG_BUTTON));
  gfx.drawString(2.5 * third, SCREEN_HEIGHT - 12, FPSTR(TEXT_REFRESH_BUTTON));
}

String getMeteoconIcon(String iconText) {
  if (iconText == "chanceflurries") return "F";
  if (iconText == "chancerain") return "Q";
  if (iconText == "chancesleet") return "W";
  if (iconText == "chancesnow") return "V";
  if (iconText == "chancetstorms") return "S";
  if (iconText == "clear") return "B";
  if (iconText == "cloudy") return "Y";
  if (iconText == "flurries") return "F";
  if (iconText == "fog") return "M";
  if (iconText == "hazy") return "E";
  if (iconText == "mostlycloudy") return "Y";
  if (iconText == "mostlysunny") return "H";
  if (iconText == "partlycloudy") return "H";
  if (iconText == "partlysunny") return "J";
  if (iconText == "sleet") return "W";
  if (iconText == "rain") return "R";
  if (iconText == "snow") return "W";
  if (iconText == "sunny") return "B";
  if (iconText == "tstorms") return "0";

  if (iconText == "nt_chanceflurries") return "F";
  if (iconText == "nt_chancerain") return "7";
  if (iconText == "nt_chancesleet") return "#";
  if (iconText == "nt_chancesnow") return "#";
  if (iconText == "nt_chancetstorms") return "&";
  if (iconText == "nt_clear") return "2";
  if (iconText == "nt_cloudy") return "Y";
  if (iconText == "nt_flurries") return "9";
  if (iconText == "nt_fog") return "M";
  if (iconText == "nt_hazy") return "E";
  if (iconText == "nt_mostlycloudy") return "5";
  if (iconText == "nt_mostlysunny") return "3";
  if (iconText == "nt_partlycloudy") return "4";
  if (iconText == "nt_partlysunny") return "4";
  if (iconText == "nt_sleet") return "9";
  if (iconText == "nt_rain") return "7";
  if (iconText == "nt_snow") return "#";
  if (iconText == "nt_sunny") return "4";
  if (iconText == "nt_tstorms") return "&";

  return ")";
}


