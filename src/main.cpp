#include <Arduino.h>
#include "main.h"
#include "FocusNinjaControl.cpp"

FocusNinjaControl focusNinja;
int state = 0;

//Test parameters
float startPosition = 20;
float stepSizemm = 0.5;
int numberOfSteps = 50;
int stepCount = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  focusNinja.motorControl();

  //test procedure
  //home, move to startposition, take 20 photos with 1 mm spacing
  switch(state){
    case 0 :  
      if (focusNinja.homed == true){
        focusNinja.moveCarriage(startPosition, FORWARDS);
        state = 1;
      }
      break;
    case 1 :
      if (not focusNinja.isMoving()){
        delayMicroseconds(500000);
        focusNinja.releaseShutter();
        delayMicroseconds(500000);
        focusNinja.moveCarriage(stepSizemm, FORWARDS);
        if (stepCount == numberOfSteps){
          state = 2;
        }  
        stepCount += 1;      
      }
      break;
  }
}