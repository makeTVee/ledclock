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

byte coordsX[NUM_LEDS] = { 184, 191, 198, 186, 171, 175, 226, 238, 236, 220, 212, 223, 241, 255, 245, 229, 229, 245, 226, 238, 223, 212, 220, 236, 184, 191, 175, 171, 186, 198, 127, 127, 114, 119, 136, 141, 71, 64, 57, 69, 84, 80, 29, 17, 19, 35, 43, 32, 14, 0, 10, 26, 26, 10, 29, 17, 32, 43, 35, 19, 71, 64, 80, 84, 69, 57, 128, 128, 141, 136, 119, 114 };
byte coordsY[NUM_LEDS] = { 172, 181, 169, 161, 167, 180, 140, 145, 133, 130, 141, 151, 97, 97, 87, 91, 103, 107, 54, 48, 43, 53, 64, 61, 22, 13, 14, 27, 33, 24, 11, 0, 7, 19, 19, 7, 22, 13, 24, 33, 27, 14, 54, 48, 61, 64, 53, 43, 97, 97, 107, 103, 91, 87, 140, 145, 151, 141, 130, 133, 172, 181, 180, 167, 161, 169, 183, 194, 186, 174, 174, 186 };
byte angles[NUM_LEDS] = { 80, 81, 81, 79, 78, 79, 82, 84, 83, 81, 81, 83, 81, 83, 81, 80, 81, 82, 78, 79, 77, 77, 78, 79, 74, 74, 73, 73, 74, 75, 69, 69, 68, 68, 70, 70, 64, 63, 63, 64, 65, 65, 60, 59, 59, 61, 62, 61, 58, 57, 57, 59, 59, 58, 59, 57, 59, 61, 60, 58, 64, 63, 66, 66, 64, 62, 73, 73, 75, 74, 71, 71 };
byte radii[NUM_LEDS] = { 152, 147, 156, 159, 152, 145, 179, 179, 186, 184, 176, 172, 208, 211, 215, 210, 202, 203, 232, 237, 238, 230, 225, 229, 246, 253, 250, 241, 239, 246, 248, 255, 249, 242, 243, 251, 239, 245, 237, 231, 236, 244, 218, 223, 214, 212, 219, 225, 191, 192, 185, 186, 194, 198, 162, 159, 155, 160, 168, 167, 140, 134, 135, 143, 147, 141, 136, 129, 135, 142, 140, 132 };

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

long loop_counter = 0;

bool off = false;
uint8_t gHue = 0;          // rotating "base color" used by many of the patterns
uint8_t touch_counter = 0; // counter for touch button (long/short press detection)

WiFiUDP ntpUDP;
uint8_t mode = 0;  
uint8_t modeCount = 2;
// offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 3600
// GMT +8 = 28800
// GMT -1 = -3600
// GMT 0 = 0
// GMT -6 = -21600
int timeOffset = 3600;
uint8_t brightness = BRIGHTNESS; 

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
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  FastLED.setBrightness(brightness);

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

  case 1: // pattern
    // call the current pattern function
    patterns[currentPatternIndex]();
    break;

  default:
    break;
  }
  
  // update the LED ring
  FastLED.delay(50);

  // do some periodic updates
  EVERY_N_MILLISECONDS(30)
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
  json += ",\"brightness\":" + String(brightness) + "";

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
  else if (name == "brightness")
  {
    brightness = value.toInt();
    FastLED.setBrightness(brightness);
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
