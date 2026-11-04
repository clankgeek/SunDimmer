#include <Arduino.h>

void energySaving();
void setDimmer(int16_t);
void router();
void setPmax(boolean);
void setClock();
void setForced();
uint8_t getNumber(String, uint8_t, uint8_t);
void lcdStrings(String, String);
String lcdHead();
void configure();
boolean question(String, String);
String getHour();
void released(uint8_t);