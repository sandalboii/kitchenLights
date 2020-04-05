#include <Arduino.h>
#include <EEPROM.h>
#include <ClickEncoder.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <BH1750.h>
#include <FastLED.h>
FASTLED_USING_NAMESPACE
//Light INIT
#define DATA_PIN D7
//#define CLK_PIN   4
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS 196
//#define NUM_LEDS    10 //TEST
CRGB leds[NUM_LEDS];
//Declare LIGHT
CHSV warmHue = CHSV(39, 84, 88);
CRGB warmColour = CRGB(255, 147, 41);
CRGB redColour = CRGB(255, 0, 0);

void fade(int val);
void lighRegular(int val);
void lighRed(int val);
int brightness;
int dynamicBrightness;
#define FRAMES_PER_SECOND 120

//LIGHT METER
BH1750 lightMeter;

//CONST
const int lightReadDelay = 1000;
const int partyDelay = 1000 / FRAMES_PER_SECOND;
const int fadeDelay = 1000;
unsigned long previousMillis = 0;

// WIFI SETUP
const char ssid[] = "";      //  your network SSID (name)
const char pass[] = ""; // your network password

// NTP Servers:
static const char ntpServerName[] = "de.pool.ntp.org";

const int timeZone = 1; // Central European Time

WiFiUDP Udp;
unsigned int localPort = 8888; // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

//ENCODER SETUP

#define ENCODER_PINA D6
#define ENCODER_PINB D5
#define ENCODER_BTN D4

#define ENCODER_STEPS_PER_NOTCH 2

ClickEncoder encoder = ClickEncoder(ENCODER_PINA, ENCODER_PINB, ENCODER_BTN, ENCODER_STEPS_PER_NOTCH);

//BoolSetup

bool motionMode = false;
bool luxDetection = true;
bool encMode = false;
bool timedDetection = true;
bool lightsOn = false;
bool dimmerOn = false;
bool partyModeOn = false;

//INIT
void setup()
{
  Serial.begin(115200);

  //  LET Setup
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(brightness);

  //  Pin Setup
  pinMode(D4, FUNCTION_0);
  pinMode(D3, INPUT); //Motion Detector
  encoder.setButtonHeldEnabled(true);
  encoder.setDoubleClickEnabled(true);
  Wire.begin(D1, D2); //Light Meter - SCL D2, SDA, D1

  lightMeter.begin();

  //NTP SET UP
  while (!Serial)
    ; // Needed for Leonardo only
  delay(250);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}
time_t prevDisplay = 0;

void loop()
{
  // ====================================
  //  LED BOOL Settings
  // ====================================
  if (lightsOn)
  {
    fade(200);
    fill_solid(leds, NUM_LEDS, warmColour);
  }
  else
  {
    fade(0);
  }

  if (encMode)
  {
    lightsOn = true;
  }
  else
  {
    lightsOn = false;
    delay(500);
    /* ----- DELAY --------*/
    unsigned long currentMillis = millis(); // Delay without Delay

    //  if (dimmerOn) {
    //    fill_solid(leds, NUM_LEDS, warmColour);
    //  }
    //  if (dimmerBrightness > 0) {
    //    dimmerOn = true;
    //  } else {
    //    dimmerOn = false;
    //  }

    //  if (partyModeOn) {
    //    brightness = 30;
    //    //PARTY MODE FUNCTION
    //    if (currentMillis - previousMillis >= partyDelay) {
    //      // Call the current pattern function once, updating the 'leds' array
    //      gPatterns[gCurrentPatternNumber]();
    //
    //      // send the 'leds' array out to the actual LED strip
    //      FastLED.show();
    //
    //      // do some periodic updates
    //      EVERY_N_MILLISECONDS( 20 ) {
    //        gHue++;  // slowly cycle the "base color" through the rainbow
    //      }
    //      EVERY_N_SECONDS( 10 ) {
    //        nextPattern();  // change patterns periodically
    //      }
    //    }
    //  } else {
    //    fade(0);
    //  }

    // ============================
    //        Light Level
    // ============================
    //Read Light Level
    if (currentMillis - previousMillis >= lightReadDelay)
    {
      previousMillis = currentMillis;
      float lux = lightMeter.readLightLevel();

      Serial.print("Light: ");
      Serial.print(lux);
      Serial.println(" lx");

      //LUX DETECTION && DYNAMIC LIGHT LEVELS
      if (luxDetection)
      {
        if (lux < 5)
        {
          motionMode = true;
          if (hour() <= 6 && lux <= 0.80)
          {
            dynamicBrightness = 2;
          }
          if (hour() >= 21 && lux <= 0.80)
          {
            dynamicBrightness = 2;
          }
          else
          {
            dynamicBrightness = 180 - (lux * 2);
          }
        }
        else if (lux > 5)
        {
          motionMode = false;
        }
      }
      else
      {
        motionMode = false;
      }
    }

    // ==========================
    //          MOTION
    // ==========================
    //  Motion Detection Loop
    int motionDetected = digitalRead(D3);
    //Motion Bool Settings
    if (motionMode)
    {
      if (motionDetected == 1)
      {
        fade(dynamicBrightness);
        // if(hour() >= 21){
        //   fill_solid(leds, NUM_LEDS, warmColour);
        // }
        fill_solid(leds, NUM_LEDS, warmColour);
      }
      else
      {
        fade(0);
      }
    }
    else
    {
      fade(0);
    }

    //TIME DETECTION
    //    if (timedDetection) {
    //      if (hour() > 23) {
    //        dynamicBrightness = 2;
    //      } if (hour() <= 6 && minute() < 30) {
    //        dynamicBrightness = 2;
    //      } else {
    //        dynamicBrightness = 10;
    //      }
    //    }
  }

  // ==========================
  //        Encoder
  // ==========================
  static uint32_t lastService = 0;
  if (lastService + 1000 < micros())
  {
    lastService = micros();
    encoder.service();
  }

  static int16_t last, value;
  value += encoder.getValue();

  if (value != last)
  {
    last = value;
    Serial.print("Encoder Value: ");
    Serial.println(value);

    if(last != brightness){
      brightness = last;
    }
  }

  ClickEncoder::Button b = encoder.getButton();
  if (b != ClickEncoder::Open)
  {
    switch (b)
    {
    //        VERBOSECASE(ClickEncoder::Pressed);
    //        VERBOSECASE(ClickEncoder::Held)
    //        VERBOSECASE(ClickEncoder::Released)
    //        VERBOSECASE(ClickEncoder::Clicked)
    //        VERBOSECASE(ClickEncoder::DoubleClicked)
    case ClickEncoder::Clicked:
      encMode = !encMode;
      break;

    case ClickEncoder::DoubleClicked:
      Serial.print("Double Clicked");
      //        partyModeOn = !partyModeOn;
      break;

    case ClickEncoder::Released:
      motionMode = !motionMode;
      if (motionMode)
      {
        //          timedDetection = false;
        luxDetection = false;
        Serial.print("\n Motion Detect ON!");
      }
      else
      {
        //                    timedDetection = true;
        luxDetection = true;
        Serial.print("\n Motion Detect OFF!");
      }
      break;
    }
  }

  // ====================================
  //           PARTY MODE
  // =====================================
  if (day() == 31 && month() == 12 && hour() == 11 && minute() == 59 && second() == 50)
  {
    //   COUNT DOWN PULSE
  }
  if (day() == 27 && month() == 1 && hour() == 21 && minute() == 45)
  {
    //    partyModeOn = true;
  }
  else
  {
    //    partyModeOn = false;
  }

  //Clock Loop
  if (timeStatus() != timeNotSet)
  {
    if (now() != prevDisplay)
    { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
}

// ==========================
//    EXTRA FUNCTIONS
// ==========================

// ==========================
// CLOCK
// ==========================

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" | ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
  Serial.print("\n");
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
// ==========================
/*-------- NTP code ----------*/
// ==========================

const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// ==========================
// LED CONTROL
// ==========================

// FADE
void fade(int val)
{
  int targetBrightness = val;
  if (targetBrightness > brightness)
  {
    brightness++;
  }
  else if (targetBrightness < brightness)
  {
    brightness--;
    if (brightness < 0)
      brightness = 0;
  }

  FastLED.setBrightness(brightness);
  FastLED.show();
}
void lightRegular(int val)
{
  fade(val);
  fill_solid(leds, NUM_LEDS, warmColour);
}
void lightRed(int val)
{
  fade(val);
  fill_solid(leds, NUM_LEDS, redColour);
}

//// TOGGLE LIGHTS
void toggleOn(int val)
{
  int targetBrightness = val;
  if (targetBrightness > brightness)
  {
    brightness++;
  }
  else if (targetBrightness < brightness)
  {
    brightness--;
    if (brightness < 0)
      brightness = 0;
  }

  FastLED.setBrightness(brightness);
  FastLED.show();
}

//Dimmer
void dimmer(int val)
{
  int dimmerBrightness = val;
  if (dimmerBrightness <= 255)
  {
    dynamicBrightness = dimmerBrightness;
  }
}

// ========================
// PARTY MODE FUNCTIONS
// ========================