#include "header.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <RBDdimmer.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>
#include <jsy-mk-194.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif

#define SERIAL_BITRATE 9600 // default is 4800, change it with jsy.changeBitrate(9600);
#define LCD_ADDRESS 0x3F    // some display use 0x27 address
#define DIMMER_PIN 5
#define ZERO_CROSS_PIN 2 // Uno is always pin 2
#define BUTTON_DIMMER_FORCED_PIN 6
#define BUTTON_SETUP_1_PIN 7
#define BUTTON_SETUP_2_PIN 8
#define EEPROM_ADDR 14 // pmax1 writed in eeprom

const String Version = "v1.0.0";

#if defined(ARDUINO_ARCH_AVR)
dimmerLamp dimmer(DIMMER_PIN);
#else
dimmerLamp dimmer(DIMMER_PIN, ZERO_CROSS_PIN);
#endif
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
ezButton button_setup1(BUTTON_SETUP_1_PIN);
ezButton button_setup2(BUTTON_SETUP_2_PIN);
RTC_DS1307 rtc;
JSY_MK_194 jsy;

// global variables
int16_t Pmax1, Pmax1_90, Watt1, Watt2 = 0;
uint8_t Nbloop = 0;
uint8_t NbloopDisplay = 20;
boolean PowerForced = false;
String Hour = "00:00";
String lcd0 = "";
String lcd1 = "";

void setup() {
  EEPROM.get(EEPROM_ADDR, Pmax1);
  if (Pmax1 < 0 || Pmax1 > 10000) { // eeprom vierge (0xFFFF) ou corrompue
    Pmax1 = 0;
  }
  pinMode(BUTTON_DIMMER_FORCED_PIN, INPUT_PULLUP);
  button_setup1.setDebounceTime(50);
  button_setup2.setDebounceTime(50);
  Serial.begin(SERIAL_BITRATE, SERIAL_8N1);
  dimmer.begin(NORMAL_MODE, OFF);
  rtc.begin();
  lcd.init();
  lcd.backlight();
  lcdStrings("SunDimmer", Version);
  energySaving();
  delay(5000);
  configure();
}

void loop() {
  if (jsy.read()) {
    if (Nbloop == 0) {
      Hour = getHour();
    }
    setForced();
    if (!PowerForced) {
      Watt1 = jsy.getPower_1(false); // Cheauffe Eau
      Watt2 = jsy.getPower_2(true); // Enedis
      router();
      if (NbloopDisplay >= 20) {
        lcdStrings(lcdHead(), "Out " + String(Watt1) + "w");
        NbloopDisplay = 0;
      }
    }
    Nbloop++;
    NbloopDisplay++;
  } else {
    setDimmer(0); // pas de mesure, on coupe par securite
    lcdStrings("JSY Erreur!", "Aucune données");
  }
  delay(50);
}

void setForced() {
  if (digitalRead(BUTTON_DIMMER_FORCED_PIN) == LOW) {
    PowerForced = true;
    dimmer.setState(ON);
    dimmer.setPower(100);
    lcdStrings(lcdHead(), "Mode manuel ON");
  } else {
    PowerForced = false;
  }
}

String getHour() {
  char cstr[8];
  sprintf(cstr, "%02d:%02d", rtc.now().hour(), rtc.now().minute());
  return cstr;
}

void configure() {
  boolean aquirePmax = (Pmax1 == 0);
  bool conf = false;
  if (digitalRead(BUTTON_SETUP_1_PIN) == LOW) {
    lcdStrings("Configuration", "...");
    while(digitalRead(BUTTON_SETUP_1_PIN) == LOW);
    conf = true;
  }
  else if (digitalRead(BUTTON_SETUP_2_PIN) == LOW) {
    while(1) {
      if (jsy.read()) {
        lcdStrings("w1 " + String(jsy.getPower_1(true)) + "w", "w2 " + String(jsy.getPower_2(true)) + "w");
      } else {
        lcdStrings("simple wattmettre", "JSY Erreur");
      }
      delay(500);
    }
    
  }
  if (conf || Pmax1 == 0) {
    if (Pmax1 > 0) {
      aquirePmax = question("Calibrer Pmax ?", "Non / Oui");
    }
    setClock();
  }

  setPmax(aquirePmax);
}

boolean question(String s1, String s2) {
  boolean ok = false;
  lcdStrings(s1, s2);
  do {
    button_setup1.loop();
    button_setup2.loop();
    if (button_setup1.isPressed()) {
      released(1);
      ok = true;
      break;
    }
    if (button_setup2.isPressed()) {
      released(2);
      ok = false;
      break;
    }
  } while (true);
  return ok;
}

void setClock() {
  int8_t hour, min = 0;
  hour = getNumber("Heure ? + / ok", 23, rtc.now().hour());
  min = getNumber("Min ? + / ok", 59, rtc.now().minute());
  rtc.adjust(DateTime(2023, 1, 1, hour, min, 0));
}

uint8_t getNumber(String mes, uint8_t max, uint8_t cur) {
  uint8_t i = cur;
  do {
    lcdStrings(mes, String(i));
    button_setup1.loop();
    button_setup2.loop();
    if (button_setup1.isPressed()) {
      released(1);
      i++;
    }
    if (i > max) {
      i = 0;
    }
  } while (!button_setup2.isPressed());
  released(2);
  return i;
}

void energySaving() {
#if !defined(ARDUINO_ARCH_AVR)
  WiFi.mode(WIFI_OFF);
#endif
#if defined(ARDUINO_ARCH_ESP32)
  setCpuFrequencyMhz(80);
#endif
}

void setDimmer(int16_t power) {
  if (power < 0 || Pmax1 <= 0) { // puissance negative ou pas calibre = 0
    power = 0;
  }
  if (power > 0) { // transformation watt en pourcent
    if (power > Pmax1_90) {     // 90% maxi
      power = Pmax1_90;
    }
    power = int16_t(int32_t(power) * 100 / Pmax1); // int32 sinon overflow sur AVR
    if (power < 10) { // 10% mini
      power = 0;
    }
  }
  if (power == 0) {
    dimmer.setPower(0);
    dimmer.setState(OFF);
  } else {
    dimmer.setState(ON);
    dimmer.setPower(power);
  }
}

void router() {
  // watt2 = Enedis flux (negatif=maison vers enedis)
  // watt1 = conso actuelle du chauffe eau
  // cible = conso actuelle + surplus injecte - 100w de marge d'erreur
  setDimmer(Watt1 - Watt2 - 100);
}

void setPmax(boolean force = false) {
  String mes0 = "";
  String mes1 = "";
  if (Pmax1 == 0 || force) {
    uint8_t xi = 128;
    uint16_t wait = 0; // timeout si le thermostat a coupe (eau chaude)
    dimmer.setState(ON);
    dimmer.setPower(100);  
    while (xi != 0 && wait < 600) {
      mes0 = "Calibration " + String(xi);
      if (jsy.read()) {
        Watt1 = jsy.getPower_1(false);
        if (Watt1 > 100) {
          if (Watt1 > Pmax1) {
            Pmax1 = Watt1;
          }
          mes1 = "Pmax = " + String(Pmax1) + "w";
          xi--;
        } else {
          mes1 = "Attente";
          wait++;
        }
      } else {
        mes1 = "JSY erreur";
        wait++;
      }
      lcdStrings(mes0, mes1);
      delay(100);
    }
    setDimmer(0);
    if (xi != 0) { // timeout, on ne sauvegarde pas
      lcdStrings("Calibration KO", "Eau chaude ?");
      delay(5000);
    } else {
      EEPROM.put(EEPROM_ADDR, Pmax1);
    }
  }
  Pmax1_90 = int16_t(Pmax1 * 0.9);
  mes0 = "eeprom ok";
  mes1 = "90%: " + String(Pmax1_90) + "w";
  lcdStrings(mes0, mes1);
  delay(5000);
}

void lcdStrings(String msg0 = "", String msg1 = "") {
  if (msg0 != lcd0 || msg1 != lcd1) {
    lcd0 = msg0;
    lcd1 = msg1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(msg0);
    lcd.setCursor(0, 1);
    lcd.print(msg1);
  } 
}

String lcdHead() {
  return String(Hour) + ", EDF " + String(Watt2) + "w";
}

void released(uint8_t b = 1) {
  if (b == 1) {
    do {
      button_setup1.loop();
    }while (!button_setup1.isReleased());
  } else {
    do {
      button_setup2.loop();
    }while (!button_setup2.isReleased());
  }
}