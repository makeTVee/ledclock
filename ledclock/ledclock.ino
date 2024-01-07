#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <AutoConnect.h>

//#define OTA

#ifdef OTA
#include <ArduinoOTA.h>
#endif

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <FS.h>
#include <SPIFFS.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "palettes.h"

WebServer Server;
AutoConnect Portal(Server);

#define NP_PIN D10    // LED ring

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 72
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 125
#define FRAMES_PER_SECOND 120
#define MILLI_AMPS 1500

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

long loop_counter = 0;

bool off = false;
uint8_t gHue = 0;          // rotating "base color" used by many of the patterns
uint8_t touch_counter = 0; // counter for touch button (long/short press detection)

WiFiUDP ntpUDP;
uint8_t mode = 0;  
uint8_t modeCount = 4;
// offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 3600
// GMT +8 = 28800
// GMT -1 = -3600
// GMT 0 = 0
// GMT -6 = -21600
int timeOffset = 3600;

NTPClient timeClient(ntpUDP, "pool.ntp.org", timeOffset, 60000);
String formattedDate;
String dayStamp;
String timeStamp;

uint8_t cyclePalette = 0;
uint8_t paletteDuration = 10;
uint8_t currentPaletteIndex = 0;
unsigned long paletteTimeout = 0;

uint8_t cyclePattern = 0;
uint8_t patternDuration = 10;
uint8_t currentPatternIndex = 0;
unsigned long patternTimeout = 0;

uint8_t speed = 30;
CRGB analogColor = CRGB::Black;

#include "patterns.h"

FASTLED_USING_NAMESPACE

// function declarations
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);

void setup()
{
  Serial.begin(115200);
  // delay(1000); // give me time to bring up serial monitor

  SPIFFS.begin();
  listDir(SPIFFS, "/", 1);

  if (Portal.begin())
  {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }

#ifdef OTA
  ArduinoOTA.onStart([]()
                     {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      Serial.println("Start updating " + type); })
      .onEnd([]()
             { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
#endif
  // setting PWM properties
  const int freq = 5000;
  const int resolution = 8;

  FastLED.addLeds<LED_TYPE, NP_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.println("Booting");

  timeClient.begin();
  timeClient.setTimeOffset(timeOffset);

  setupWeb();
}

void loop()
{
  Portal.handleClient();
#ifdef OTA
  ArduinoOTA.handle();
#endif

  switch (mode)
  {
  case 0: // clock
    drawClock();
    break;

  case 3: // pattern
    // call the current pattern function
    patterns[currentPatternIndex]();
    break;

  default:
    break;
  }
  
  // update the LED ring
  FastLED.delay(50);

  // do some periodic updates
  EVERY_N_MILLISECONDS(1000)
  {
    gHue++; // slowly cycle the "base color" through the rainbow
  }

  EVERY_N_MILLISECONDS(40)
  {
    // slowly blend the current palette to the next
    nblendPaletteTowardPalette(currentPalette, targetPalette, 8);
    gHue++; // slowly cycle the "base color" through the rainbow
  }

  // change to a new palette
  if (cyclePalette == 1 && (millis() > paletteTimeout))
  {
    currentPaletteIndex = (currentPaletteIndex + 1) % paletteCount;
    targetPalette = palettes[currentPaletteIndex];
    paletteTimeout = millis() + (paletteDuration * 1000);
  }

  // change to a new pattern
  if (cyclePattern == 1 && (millis() > patternTimeout))
  {
    currentPatternIndex = (currentPatternIndex + 1) % patternCount;
    patternTimeout = millis() + (patternDuration * 1000);
  }
}

void set_time(uint8_t hour, uint8_t minute, uint8_t second)
{
  FastLED.clear();
  //hours
  gHue=0;
  uint8_t min = minute;
  uint8_t hor=hour%12;
  if (hor ==0) hor =12;
  for (int h=0;h<hor;h++)
  {
      leds[h*6]=CRGB::YellowGreen;
  }

  for (int h=0;h<12;h++)
  {
   for (int m = 0; m<5;m++)
      {
        if (min > 0)
        {
          leds[h*6+1+m]=CHSV( gHue, 200, 120);
          gHue+=5;
           min--;
        }
      
      }
  }
  //leds[(hour%12)*6+1+(minute%5)]=CHSV(gHue, 200, second*2);
  //leds[(hour % 12) * 5 + (minute / 12)] = CRGB::Blue;
  //leds[minute] += CRGB::Red;
  //leds[second] += CRGB::Green;
}

void drawClock()
{
  timeClient.update();
  formattedDate = timeClient.getFormattedTime();
  int splitT = formattedDate.indexOf("T");
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length());
  set_time(timeStamp.substring(0, 2).toInt(), timeStamp.substring(3, 5).toInt(), timeStamp.substring(6, 8).toInt());
}


void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

String getData()
{
  String json = "{";
 
  json += "\"mode\":" + String(mode) + "";
  json += ",\"speed\":" + String(speed) + "";
  json += ",\"timeOffset\":" + String(timeOffset) + "";

  json += ",\"cyclePalette\":" + String(cyclePalette) + "";
  json += ",\"paletteDuration\":" + String(paletteDuration) + "";
  json += ",\"currentPaletteIndex\":" + String(currentPaletteIndex) + "";

  json += ",\"cyclePattern\":" + String(cyclePattern) + "";
  json += ",\"patternDuration\":" + String(patternDuration) + "";
  json += ",\"currentPatternIndex\":" + String(currentPatternIndex) + "";

  json += "}";

  return json;
}

void setValue()
{
  bool ok = true;

  String name = Server.arg("name");
  String value = Server.arg("value");
  
    if (name == "mode")
  {
    mode = value.toInt();
    if (mode < 0)
      mode = 0;
    else if (mode >= modeCount)
      mode = modeCount - 1;
  }
  else if (name == "timeOffset")
  {
    timeClient.setTimeOffset(value.toInt());
  }
  else if (name == "cyclePalette")
  {
    cyclePalette = value.toInt();
  }
  else if (name == "paletteDuration")
  {
    paletteDuration = value.toInt();
  }
  else if (name == "currentPaletteIndex")
  {
    currentPaletteIndex = value.toInt();
    if (currentPaletteIndex < 0)
      currentPaletteIndex = 0;
    else if (currentPaletteIndex >= paletteCount)
      currentPaletteIndex = paletteCount - 1;
    // change palette immediately, so set both, rather than letting them cross-fade
    targetPalette = palettes[currentPaletteIndex];
    currentPalette = palettes[currentPaletteIndex];
    paletteTimeout = millis() + (paletteDuration * 1000);
  }
  else if (name == "speed")
  {
    speed = value.toInt();
  }
  else if (name == "cyclePattern")
  {
    cyclePattern = value.toInt();
  }
  else if (name == "patternDuration")
  {
    patternDuration = value.toInt();
  }
  else if (name == "currentPatternIndex")
  {
    currentPatternIndex = value.toInt();
    if (currentPatternIndex < 0)
      currentPatternIndex = 0;
    else if (currentPatternIndex >= patternCount)
      currentPatternIndex = patternCount - 1;
    patternTimeout = millis() + (patternDuration * 1000);
  }
  else
  {
    ok = false;
  }

  if (ok)
    Server.send(200);
  else
    Server.send(404, "text/plain", "Unknown field");
}

void setupWeb()
{
  Server.enableCORS(true);
  Server.on("/data", HTTP_GET, []()
            { Server.send(200, "text/json", getData()); });
  Server.on("/set", HTTP_POST, []()
            { setValue(); });

  Server.serveStatic("/", SPIFFS, "/index.htm");
  Server.serveStatic("/app.js", SPIFFS, "/app.js");
}
