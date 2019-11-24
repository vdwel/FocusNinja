#include <Arduino.h>
#include "main.h"
#include "FocusNinjaControl.cpp"

FocusNinjaControl focusNinja;
int state = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  focusNinja.motorControl();

  //test procedure
  switch(state){
    case 0 :  
      if (focusNinja.homed == true){
        focusNinja.moveCarriage(25, FORWARDS);
        state = 1;
      }
    case 1 :
      if (not focusNinja.moving()){
        focusNinja.releaseShutter();
        state = 2;
      }
  }
}