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

WebServer Server;         
AutoConnect      Portal(Server);

#define NP_PIN    12  //LED ring
#define RED_PIN   26  //Red LED for PWM
#define GREEN_PIN 27  //Green LED for PWM
#define BLUE_PIN  25  //Blue LED for PWM
#define TOUCH_PIN 4   //Touch button
#define BUZZ_PIN  33  // Buzzer (just on off, no freq needed)

#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    60
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          45
#define FRAMES_PER_SECOND  120
#define MILLI_AMPS         1500

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// setting PWM properties
const int freq = 5000;
const int resolution = 8;

bool off=false;
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 160; // rotating "base color" used by many of the patterns
uint8_t gcnt = 0;
uint8_t led_state=0;      //status of the 3 LEDs in the mid (7 permutations out of 3 LEDs: R,G,B,RG,RB,GB,RGB)
uint8_t duty_cycle=0;     //duty cycle for PWM 
uint8_t led_dir=0;        //fading direction for PWM
uint8_t touch_counter=0;  //counter for touch button (long/short press detection)
uint8_t sys_state=0;      //state machine variable for different modes (LED only, clock, timer ...)
uint8_t hue=0;
bool touch_long_press=false;
bool touch_short_press=false;
bool timer_runs=false;    //timer mode status
bool transition=false;    //transition for state change
long loop_counter=0;
uint16_t timer_cnt=1;


WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000);
String formattedDate;
String dayStamp;
String timeStamp;

const uint8_t touch_threshold = 72;   //threshold for touch detection
bool touched= false;                  // Touch 0 GPIO4
void IRAM_ATTR T0Activated() { touched = true; }
#define TOUCH_LONG_TIME 50            
#define TOUCH_SHORT_TIME 15

FASTLED_USING_NAMESPACE

void rootPage() {
  char content[] = "Hello WS2812B clock";
  Server.send(200, "text/plain", content);
}

//Function declaration
void rainbow();
void addGlitter(fract8 chanceOfGlitter);
void rainbowWithGlitter();
void rgb_pwm();
void set_time(uint8_t hour, uint8_t minute, uint8_t second);
void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }
void plot_timer(uint16_t cnt);
void confetti() ;

void setup() {
  Serial.begin(115200);
  //delay(1000); // give me time to bring up serial monitor
  
    if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    }

  #ifdef OTA
     ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
  #endif


  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  ledcSetup(0, freq, resolution);
  ledcSetup(1, freq, resolution);
  ledcSetup(2, freq, resolution);
  ledcAttachPin(RED_PIN, 0);
  ledcAttachPin(GREEN_PIN, 1);
  ledcAttachPin(BLUE_PIN, 2);
  ledcWrite(0, 255); 
  ledcWrite(1, 255); 
  ledcWrite(2, 255); 

  touchAttachInterrupt(TOUCH_PIN, T0Activated, touch_threshold);

  FastLED.addLeds<LED_TYPE,NP_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.println("Booting");

  /*WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }*/

  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(-21600);
  
}
int splitT;
void loop() {
  // Call the current pattern function once, updating the 'leds' array
  Portal.handleClient();
  #ifdef OTA
    ArduinoOTA.handle(); 
  #endif

  switch (sys_state)
  {
    case 0:
      FastLED.clear();
      FastLED.show();
      break;
    case 1:
      if (transition)
      {
        
                // First slide the led in one direction
          for(int i = 0; i < NUM_LEDS; i++) {
            // Set the i'th led to red 
            hue+=4;
            leds[i] = CHSV(hue, 255, 255);
            // Show the leds
            FastLED.show(); 
            // now that we've shown the leds, reset the i'th led to black
            // leds[i] = CRGB::Black;
            fadeall();
            // Wait a little bit before we loop around and do it again
            delay(25);
        }
        transition=false;
      }
      timeClient.update();
      formattedDate = timeClient.getFormattedTime();
      splitT = formattedDate.indexOf("T");
      timeStamp = formattedDate.substring(splitT+1, formattedDate.length());
      set_time(timeStamp.substring(0,2).toInt(), timeStamp.substring(3,5).toInt(), timeStamp.substring(6,8).toInt());
      break;
    case 2:
      if (transition)
      {
         loop_counter=0;
         timer_runs=false;
         timer_cnt=1;
         for(int i = NUM_LEDS-1; i >= 0; i--) {
            hue+=4;
            leds[i] = CHSV(hue, 255, 255);
            FastLED.show(); 
            fadeall();
            delay(25);
         }
        transition=false;
        FastLED.clear();
        FastLED.show();
      }
      loop_counter++;
      if (touch_short_press && (!timer_runs))
      {
        timer_cnt=(timer_cnt+1)%60;
        plot_timer((timer_cnt-1)*60);
        touch_short_press=false;
        loop_counter=0;
      }
      if(loop_counter >=100)
      {
        timer_runs = true;
        timer_cnt= (timer_cnt-1)*60;
        loop_counter=0;
      }
      if(timer_runs)
      {
        plot_timer(timer_cnt);
        if (loop_counter==20)
        {
          timer_cnt--;
          loop_counter=0;
          if (timer_cnt==0)
          {
            for (int r=0;r<5;r++)
            {
              for (int b=0;b<4;b++)
              {
                digitalWrite(BUZZ_PIN,HIGH);
                delay(100);
                digitalWrite(BUZZ_PIN,LOW);
                delay(100);
              }
            delay(500);
            sys_state=0;
            }
          timer_runs=false;
          }
        }
        //Serial.println(timer_cnt);
      }
      break;
    case 3:
      if (transition)
          {
            loop_counter=0;
            transition = false;
          }
        leds[loop_counter] = CHSV(hue++, 255, 255);
        // Show the leds
        FastLED.show(); 
        // now that we've shown the leds, reset the i'th led to black
        // leds[i] = CRGB::Black;
        fadeall();
        fadeall();
        // Wait a little bit before we loop around and do it again
    

      loop_counter=(loop_counter+1)%60;
      break;
    default:
      break;
  }

  if (touch_long_press)
  {
    sys_state++;
    transition = true;
    if (sys_state==4) sys_state=0;
    touch_long_press=false;
  }

  if (touched)
  {
      touched = 0;
      touch_counter++;
      if (touch_counter==TOUCH_SHORT_TIME)
      {
        touch_short_press=true;
      }
      if (touch_counter==TOUCH_LONG_TIME)
      {
        touch_long_press=true;
        digitalWrite(BUZZ_PIN,HIGH);
        delay(200);
        digitalWrite(BUZZ_PIN,LOW);
      }
  }
  else
  {
    touch_counter=0;
  }
  FastLED.delay(50); 
  //Serial.println(touch_counter);

  // do some periodic updates
  EVERY_N_MILLISECONDS( 5 ) 
  { 
    gHue++;
    if (led_dir==0)
      {
      duty_cycle+=5;
      if (duty_cycle==250) 
        {
          led_dir=1;
        }
      }
    else
      {
      duty_cycle-=5;
      if (duty_cycle==0) 
        {
          led_dir=0;
          led_state++;
          if (led_state==7) led_state=0;
        }
      }
    rgb_pwm();   

  } // slowly cycle the "base color" through the rainbow
 

}

void set_time(uint8_t hour, uint8_t minute, uint8_t second)
{
  FastLED.clear();
  leds[(hour%12)*5+(minute/12)] = CRGB::Blue;
  leds[(((hour%12)*5+(minute/12))+1)%60] = 0x00000030;
  leds[(((hour%12)*5+(minute/12))-1)%60] = 0x00000030;
  leds[minute] += CRGB::Red;
  leds[(minute-1)%60] += 0x00300000;
  leds[second] += CRGB::Green;
  FastLED.show();  
}


void rgb_pwm()
{
  switch(led_state)
  {
    case 0: 
      ledcWrite(0, 255-duty_cycle);
      ledcWrite(1, 255);
      ledcWrite(2, 255);
      break;
    case 1: 
      ledcWrite(1, 255-duty_cycle);
      ledcWrite(0, 255);
      ledcWrite(2, 255);
      break;
    case 2: 
      ledcWrite(2, 255-duty_cycle);
      ledcWrite(0, 255);
      ledcWrite(1, 255);
      break;
    case 3: 
      ledcWrite(0, 255-duty_cycle);
      ledcWrite(1, 255-duty_cycle);
      ledcWrite(2, 255);
      break;
    case 4: 
      ledcWrite(1, 255-duty_cycle);
      ledcWrite(2, 255-duty_cycle);
      ledcWrite(0, 255);
      break;
    case 5: 
      ledcWrite(0, 255-duty_cycle);
      ledcWrite(2, 255-duty_cycle);
      ledcWrite(1, 255);
      break;
    case 6: 
      ledcWrite(0, 255-duty_cycle);
      ledcWrite(1, 255-duty_cycle);
      ledcWrite(2, 255-duty_cycle);
      break;
    default:
      break;

  }
}

void plot_timer(uint16_t cnt)
{
    uint8_t minute = cnt / 60;
    uint8_t second = cnt % 60;
    FastLED.clear();
    for (int i=0; i< minute; i++)
    {
      leds[i]= 0xCCCCCC;
    }
    for (int i=0; i< second; i++)
    {
      leds[i]+= 0x202020;
    }
    FastLED.show();
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}