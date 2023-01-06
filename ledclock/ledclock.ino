#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <AutoConnect.h>

#define OTA

#ifdef OTA
#include <ArduinoOTA.h>
#endif

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <FS.h>
#include <SPIFFS.h>

WebServer Server;
AutoConnect Portal(Server);

#define NP_PIN 12    // LED ring
#define RED_PIN 26   // Red LED for PWM
#define GREEN_PIN 27 // Green LED for PWM
#define BLUE_PIN 25  // Blue LED for PWM
#define TOUCH_PIN 4  // Touch button
#define BUZZ_PIN 33  // Buzzer (just on off, no freq needed)

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 45
#define FRAMES_PER_SECOND 120
#define MILLI_AMPS 1500

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

bool off = false;
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0;                  // rotating "base color" used by many of the patterns
uint8_t touch_counter = 0;         // counter for touch button (long/short press detection)
uint8_t sys_state = 5;             // state machine variable for different modes (LED only, clock, timer ...)
uint8_t hue = 0;
bool touch_long_press = false;
bool touch_short_press = false;
bool timer_runs = false; // timer mode status
bool transition = false; // transition for state change
long loop_counter = 0;
uint16_t timer_cnt = 1;

WiFiUDP ntpUDP;

// offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 3600
// GMT +8 = 28800
// GMT -1 = -3600
// GMT 0 = 0
// GMT -6 = -21600
int timeOffset = -21600;

NTPClient timeClient(ntpUDP, "pool.ntp.org", timeOffset, 60000);
String formattedDate;
String dayStamp;
String timeStamp;

const uint8_t touch_threshold = 72; // threshold for touch detection
bool touched = false;               // Touch 0 GPIO4
void IRAM_ATTR T0Activated() { touched = true; }
#define TOUCH_LONG_TIME 50
#define TOUCH_SHORT_TIME 15

FASTLED_USING_NAMESPACE

// function declarations
void addGlitter(fract8 chanceOfGlitter);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
void plot_timer(uint16_t cnt);
void updateAnalogLeds();
void set_time(uint8_t hour, uint8_t minute, uint8_t second);
void setupWeb();

// pattern function declarations
void drawAnimation();
void drawClock();
void drawSolidColor();
void drawTimer();
void confetti();
void rainbow();
void rainbowWithGlitter();
void pride();

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

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  // setting PWM properties
  const int freq = 5000;
  const int resolution = 8;

  ledcSetup(0, freq, resolution);
  ledcSetup(1, freq, resolution);
  ledcSetup(2, freq, resolution);
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);

  // turn off the analog LEDs
  updateAnalogLeds(CRGB::Black);

  touchAttachInterrupt(TOUCH_PIN, T0Activated, touch_threshold);

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

  CRGB analogColor = CHSV(gHue, 255, 255);

  switch (sys_state)
  {
  case 0: // off
    FastLED.clear();
    FastLED.show();
    analogColor = CRGB::Black;
    break;

  case 1: // clock
    drawClock();
    break;

  case 2: // timer
    drawTimer();
    break;

  case 3: // animation
    drawAnimation();
    // have the analog LEDs match the color of the first LED
    analogColor = leds[0];
    break;

  case 4: // solid color, same as the analog LEDs
    drawSolidColor();
    break;

  case 5: // pride
    pride();
    // have the analog LEDs match the color of the first LED
    analogColor = leds[0];
    break;

  default:
    break;
  }

  if (touch_long_press) // next state
  {
    sys_state++;
    transition = true;
    if (sys_state == 6)
      sys_state = 0;
    touch_long_press = false;
  }

  if (touched)
  {
    touched = 0;
    touch_counter++;
    if (touch_counter == TOUCH_SHORT_TIME)
    {
      touch_short_press = true;
    }
    if (touch_counter == TOUCH_LONG_TIME)
    {
      touch_long_press = true;
      digitalWrite(BUZZ_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZ_PIN, LOW);
    }
  }
  else
  {
    touch_counter = 0;
  }

  // Serial.println(touch_counter);

  // update the LED ring
  FastLED.delay(50);

  // do some periodic updates
  EVERY_N_MILLISECONDS(30)
  {
    gHue++; // slowly cycle the "base color" through the rainbow
    updateAnalogLeds(analogColor);
  }
}

void set_time(uint8_t hour, uint8_t minute, uint8_t second)
{
  FastLED.clear();
  leds[(hour % 12) * 5 + (minute / 12)] = CRGB::Blue;
  leds[(((hour % 12) * 5 + (minute / 12)) + 1) % 60] = 0x00000030;
  leds[(((hour % 12) * 5 + (minute / 12)) - 1) % 60] = 0x00000030;
  leds[minute] += CRGB::Red;
  leds[(minute - 1) % 60] += 0x00300000;
  leds[second] += CRGB::Green;
  FastLED.show();
}

void updateAnalogLeds(CRGB rgb)
{
  ledcWrite(0, 255 - rgb.r);
  ledcWrite(1, 255 - rgb.g);
  ledcWrite(2, 255 - rgb.b);
}

void plot_timer(uint16_t cnt)
{
  uint8_t minute = cnt / 60;
  uint8_t second = cnt % 60;
  FastLED.clear();
  for (int i = 0; i < minute; i++)
  {
    leds[i] = 0xCCCCCC;
  }
  for (int i = 0; i < second; i++)
  {
    leds[i] += 0x202020;
  }
  FastLED.show();
}

void drawAnimation()
{
  if (transition)
  {
    loop_counter = 0;
    transition = false;
  }
  leds[loop_counter] = CHSV(hue++, 255, 255);

  // Show the leds
  FastLED.show();

  fadeToBlackBy(leds, NUM_LEDS, 13);

  // Wait a little bit before we loop around and do it again
  loop_counter = (loop_counter + 1) % 60;
}

void drawClock()
{
  if (transition)
  {
    // First slide the led in one direction
    for (int i = 0; i < NUM_LEDS; i++)
    {
      // Set the i'th led to hue
      hue += 4;
      leds[i] = CHSV(hue, 255, 255);

      // Show the leds
      FastLED.show();

      fadeToBlackBy(leds, NUM_LEDS, 10);

      // Wait a little bit before we loop around and do it again
      delay(25);
    }
    transition = false;
  }
  timeClient.update();
  formattedDate = timeClient.getFormattedTime();
  int splitT = formattedDate.indexOf("T");
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length());
  set_time(timeStamp.substring(0, 2).toInt(), timeStamp.substring(3, 5).toInt(), timeStamp.substring(6, 8).toInt());
}

void drawSolidColor()
{
  if (transition)
  {
    loop_counter = 0;
    transition = false;
  }
  fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
  FastLED.show();
  loop_counter = (loop_counter + 1) % 60;
}

void drawTimer()
{
  if (transition)
  {
    loop_counter = 0;
    timer_runs = false;
    timer_cnt = 1;
    for (int i = NUM_LEDS - 1; i >= 0; i--)
    {
      hue += 4;
      leds[i] = CHSV(hue, 255, 255);
      FastLED.show();
      fadeToBlackBy(leds, NUM_LEDS, 10);
      delay(25);
    }
    transition = false;
    FastLED.clear();
    FastLED.show();
  }
  loop_counter++;
  if (touch_short_press && (!timer_runs))
  {
    timer_cnt = (timer_cnt + 1) % 60;
    plot_timer((timer_cnt - 1) * 60);
    touch_short_press = false;
    loop_counter = 0;
  }
  if (loop_counter >= 100)
  {
    timer_runs = true;
    timer_cnt = (timer_cnt - 1) * 60;
    loop_counter = 0;
  }
  if (timer_runs)
  {
    plot_timer(timer_cnt);
    if (loop_counter == 20)
    {
      timer_cnt--;
      loop_counter = 0;
      if (timer_cnt == 0)
      {
        for (int r = 0; r < 5; r++)
        {
          for (int b = 0; b < 4; b++)
          {
            digitalWrite(BUZZ_PIN, HIGH);
            delay(100);
            digitalWrite(BUZZ_PIN, LOW);
            delay(100);
          }
          delay(500);
          sys_state = 0;
        }
        timer_runs = false;
      }
    }
    // Serial.println(timer_cnt);
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void addGlitter(fract8 chanceOfGlitter)
{
  if (random8() < chanceOfGlitter)
  {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t sat8 = beatsin88(43.5, 220, 250);
  // uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint8_t brightdepth = beatsin88(171, 96, 224);
  // uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint16_t brightnessthetainc16 = beatsin88(102, (25 * 256), (40 * 256));
  // uint8_t msmultiplier = beatsin88(147, 23, 60);
  uint8_t msmultiplier = beatsin88(74, 23, 60);

  uint16_t hue16 = sHue16; // gHue * 256;
  // uint16_t hueinc16 = beatsin88(113, 1, 3000);
  uint16_t hueinc16 = beatsin88(57, 1, 128);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis;
  sLastMillis = ms;
  sPseudotime += deltams * msmultiplier;
  // sHue16 += deltams * beatsin88( 400, 5, 9);
  sHue16 += deltams * beatsin88(200, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for (uint16_t i = 0; i < NUM_LEDS; i++)
  {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16 = sin16(brightnesstheta16) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV(hue8, sat8, bri8);

    uint16_t pixelnumber = i;

    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend(leds[pixelnumber], newcolor, 64);
  }
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
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

  json += "\"mode\":" + String(sys_state) + "";
  json += ",\"timeOffset\":" + String(timeOffset) + "";

  json += "}";

  return json;
}

void setupWeb()
{
  Server.enableCORS(true);
  Server.on("/data", HTTP_GET, []()
            { Server.send(200, "text/json", getData()); });
  Server.on("/set", HTTP_POST, []()
            {
    String name = Server.arg("name");
    String value = Server.arg("value");
    
    if (name == "mode")
    {
      sys_state = value.toInt();
      sys_state = max(sys_state, (uint8_t)0);
      sys_state = min(sys_state, (uint8_t)3);
      transition = true;
      touch_long_press = false;
      Server.send(200);
    } else if (name == "timeOffset")
    {
      timeClient.setTimeOffset(value.toInt());
      Server.send(200);
    }
    else
    {
      Server.send(404, "text/plain", "Unknown field");
    } });

  Server.serveStatic("/", SPIFFS, "/index.htm");
  Server.serveStatic("/app.js", SPIFFS, "/app.js");
}
