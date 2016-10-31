#include <Adafruit_NeoPixel.h>

/**
 * This code uses the rainbow code from the examples 
 * of the sample files in the Adafruit Neopixel library.
 * 
 * Keep up the good work, Adafruit !
 */

#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
#include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

#define trigPin 10
#define echoPin 6
#define sampleSize 50


#define BASE_RED 0
#define BASE_GREEN 0
#define BASE_BLUE 75
#define BASE_INTENSITY 20

#define PIN            6

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      50

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

int32_t lightServiceId;
int32_t lightIntensityCharId;
int32_t lightIntensity;
int32_t * previousLightIntensity;
int32_t lightColorCharId;
int32_t lightColor;
int32_t lightModeCharId;
int32_t lightMode;

char * colorRed = new char[3];
char * colorGreen = new char[3];
char * colorBlue = new char[3];

char * workRed = new char[3];
char * workGreen = new char[3];
char * workBlue = new char[3];

char red = 0;
char green = 0;
char blue = 0;


int count = 0;
float distanceWeighed = 0;


// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void setup() {
  while (!Serial); // required for Flora & Micro
  delay(500);

  initBLE();
  Serial.begin(9600);

  pixels.begin();
  pixels.setBrightness(150);

  previousLightIntensity = 0;
}

void loop() {
  lightIntensity = readIntChar(lightIntensityCharId);
  lightMode = readIntChar(lightModeCharId);
  readColor(lightColorCharId);
  int red = strtol(workRed, NULL, 16);
  int green = strtol(workGreen, NULL, 16);
  int blue = strtol(workBlue, NULL, 16);
  Serial.print(red);
  Serial.print(" ");
  Serial.print(green);
  Serial.print(" ");
  Serial.println(blue);
  if (lightMode > 3) {
    lightMode = random(0, 2);
  }

  if (red == 0 && green == 0 && blue == 0) {
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, BASE_RED, BASE_GREEN, BASE_BLUE);
    }
    pixels.show();
  } else {
    if (lightMode == 0) {
      pulse(red, green, blue, 255, 2, 250, 100);
    } else if (lightMode == 1) {
      rotate(red, green, blue, 40, 20, 5);
    } else if (lightMode == 2) {
      rotateDark(red, green, blue, 20, 20, 5);
    } else {
      rainbowCycle(10);
    }

    Serial.print("Lightmode: ");
    Serial.println(lightMode);

    resetColorCharacteristic();
  }

  if (lightIntensity != *previousLightIntensity) {
    Serial.print("Light instensity: ");
    Serial.print(lightIntensity);
    Serial.print(" ");
    Serial.print(*previousLightIntensity);
    Serial.println(lightMode);
    pixels.setBrightness(lightIntensity);
    *previousLightIntensity = lightIntensity;
  }
}

void initBLE() {
  boolean success;

  //Serial.println(F("blue-helmet"));
  //Serial.println(F("-----------"));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }

  /* Perform a factory reset to make sure everything is in a known state */
  //Serial.println(F("Performing a factory reset: "));
  if (! ble.factoryReset() ) {
    error(F("Couldn't factory reset"));
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  //Serial.println(F("Requesting Bluefruit info:"));
  /* Print Bluefruit information */
  //ble.info();

  // this line is particularly required for Flora, but is a good idea
  // anyways for the super long lines ahead!
  // ble.setInterCharWriteDelay(5); // 5 ms

  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=blue-helmet")) ) {
    error(F("Could not set device name?"));
  }

  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0xFF10"), &lightServiceId);
  if (! success) {
    error(F("Could not add service"));
  }

  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0xFF11, PROPERTIES=0x08, MIN_LEN=1, MAX_LEN=1, VALUE=150"), &lightIntensityCharId);
  if (! success) {
    error(F("Could not add characteristic"));
  }

  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0xFF12, PROPERTIES=0x08, MIN_LEN=3, MAX_LEN=3, VALUE=00-00-00"), &lightColorCharId);
  if (! success) {
    error(F("Could not add characteristic"));
  }

  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0xFF13, PROPERTIES=0x08, MIN_LEN=1, MAX_LEN=1, VALUE=0"), &lightModeCharId);
  if (! success) {
    error(F("Could not add characteristic"));
  }

  /* Add the Service to the advertising data*/
  //Serial.print(F("Adding Service UUID to the advertising payload: "));
  ble.sendCommandCheckOK( F("AT+GAPSETADVDATA=02-01-06-03-02-10-ff") );

  /* Reset the device for the new service setting changes to take effect */
  ble.reset();
}

int32_t readColor(int32_t charId) {
  // This is overkill but I was too lazy to upgrade to the latest version
  // of the firmware for the Bluetooth chip as I didn't have the time to 
  // figure out how and Halloween was coming closely.
  String command = "AT+GATTCHAR=";
  command += charId;
  char commandChr[15];
  command.toCharArray(commandChr, 15);
  ble.println(commandChr);
  ble.readline();

  String str = ble.buffer;
  str.substring(0, 2).toCharArray(workRed, 3);
  str.substring(3, 5).toCharArray(workGreen, 3);
  str.substring(6, 8).toCharArray(workBlue, 3);
}

int32_t readIntChar(int32_t charId) {
  String command = "AT+GATTCHAR=";
  command += charId;
  char commandChr[15];
  command.toCharArray(commandChr, 15);
  ble.println(commandChr);
  ble.readline();

  return (int)strtol(ble.buffer, NULL, 16);
}

void updateIntCharacteristic(Adafruit_BluefruitLE_SPI* ble, int charId, int charValue) {
  ble->print( F("AT+GATTCHAR=") );
  ble->print( charId );
  ble->print( F(",") );
  ble->println( charId );
}

void resetColorCharacteristic() {
  ble.print( F("AT+GATTCHAR=") );
  ble.print( lightColorCharId );
  ble.println( F(",00-00-00") );
}

void pulse(int targetRed, int targetGreen, int targetBlue, int targetIntensity, int times, int stepPause, int pulsePause) {
  float steps = 25;

  float stepR = (BASE_RED - targetRed) / steps;
  float stepG = (BASE_GREEN - targetGreen) / steps;
  float stepB = (BASE_BLUE - targetBlue) / steps;
  float stepIntensity = (BASE_INTENSITY - targetIntensity) / steps;

  float red = BASE_RED;
  float green = BASE_GREEN;
  float blue = BASE_BLUE;
  float intensity = BASE_INTENSITY;

  for (int i = 0 ; i < times ; i++) {
    for (int s = 0 ; s < steps * 2 ; s++) {

      Serial.print(red);
      Serial.print(" ");
      Serial.print(green);
      Serial.print(" ");
      Serial.print(blue);
      Serial.print(" ");
      Serial.println(intensity);

      for (int l = 0 ; l < NUMPIXELS ; l++) {
        pixels.setPixelColor(l, red, green, blue);
      }

      //pixels.setBrightness(intensity);

      if (s == steps) {
        stepR = stepR * -1;
        stepG = stepG * -1;
        stepB = stepB * -1;
        stepIntensity = stepIntensity * -1;
        delay(stepPause);
      }
      red = red - stepR;
      green = green - stepG;
      blue = blue - stepB;
      intensity = intensity - stepIntensity;



      pixels.show();
    }

    stepR = stepR * -1;
    stepG = stepG * -1;
    stepB = stepB * -1;
    stepIntensity = stepIntensity * -1;

    delay(pulsePause);
  }

}

void rotate(int red, int green, int blue, int width, int rotations, int pause) {
  for (int i = 0 ; i < NUMPIXELS ; i++) {
    pixels.setPixelColor(i, red, green, blue);
    pixels.show();
    delay(pause / 2);
  }

  for (int i = 0 ; i < rotations ; i++) {
    for (int j = 0 ; j < NUMPIXELS ; j++) {
      pixels.setPixelColor((j + width) % NUMPIXELS, red, green, blue);
      pixels.setPixelColor(j, 255, 255, 255);
      pixels.show();
      delay(pause);
    }
  }
  for (int i = 0 ; i < NUMPIXELS ; i++) {
    pixels.setPixelColor(i, BASE_RED, BASE_GREEN, BASE_BLUE);
    pixels.show();
    delay(pause / 2);
  }

}

void rotateDark(int red, int green, int blue, int width, int rotations, int pause) {
  for (int i = 0 ; i < NUMPIXELS ; i++) {
    pixels.setPixelColor(i, red, green, blue);
    pixels.show();
    delay(pause / 2);
  }

  for (int i = 0 ; i < rotations ; i++) {
    for (int j = 0 ; j < NUMPIXELS ; j++) {
      pixels.setPixelColor((j + width) % NUMPIXELS, red, green, blue);
      pixels.setPixelColor(j, 0, 0, 0);
      pixels.show();
      delay(pause);
    }
  }
  for (int i = 0 ; i < NUMPIXELS ; i++) {
    pixels.setPixelColor(i, BASE_RED, BASE_GREEN, BASE_BLUE);
    pixels.show();
    delay(pause / 2);
  }

}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(j));
    }
    pixels.show();
    delay(wait);
  }

  for (int i = 0 ; i < NUMPIXELS ; i++) {
    pixels.setPixelColor(i, BASE_RED, BASE_GREEN, BASE_BLUE);
    pixels.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
