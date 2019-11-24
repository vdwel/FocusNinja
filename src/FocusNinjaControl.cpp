#include "FocusNinjaControl.h"
#include <Arduino.h>

class FocusNinjaControl
{
    public:
        bool homed = false;
        float position = 0;

        //Initialization code
        FocusNinjaControl(){
            pinMode(LED_BUILTIN, OUTPUT);
            pinMode(PIN_ENDSTOP,INPUT_PULLDOWN);
            pinMode(PIN_DIRECTION, OUTPUT); 
            pinMode(PIN_STEP, OUTPUT);
            pinMode(PIN_CAMERA_RELEASE, OUTPUT);
            digitalWrite(PIN_CAMERA_RELEASE, LOW);
            homeCarriage();
        }

        void moveCarriage(float milimeter, bool direction){
            int degreesToRotate = (milimeter/MILIMETER_PER_ROTATION)*360;
            if (direction == FORWARDS) {
                carriageDirection = 1;
            } else {
                carriageDirection = -1;
            }
            rotateStepper(degreesToRotate, DEFAULT_SPEED, direction);
        }

        //Home the carriage
        void homeCarriage(){
            homed = false;
        }

        bool moving(){
            return numberOfPulses > 0;
        }

        void releaseShutter(){
            digitalWrite(PIN_CAMERA_RELEASE, HIGH);
            delayMicroseconds(20000);
            digitalWrite(PIN_CAMERA_RELEASE, LOW);
        }

        //This needs to be called from the loop
        void motorControl() {
            //Minumum position reached, stop moving
            if (position <= 0 and carriageDirection == -1){
                numberOfPulses = 0;
            }
            //Maximum position reached, stop moving
            if (position >= MAX_POSITION and carriageDirection == 1){
                numberOfPulses = 0;
            }
            //Move the carriage
            if (numberOfPulses > 0){
                numberOfPulses -= 1;
                position = position + (mmPerStep * carriageDirection);
                digitalWrite(PIN_STEP, HIGH);
                delayMicroseconds(pulseWidthMs);
                digitalWrite(PIN_STEP, LOW);
                delayMicroseconds(pulseWidthMs);        
            }
            //Read endstop en feedback to led
            bool endStop = digitalRead(PIN_ENDSTOP);
            digitalWrite(LED_BUILTIN, endStop);
            //If homing, stop when endstop triggers
            if (endStop == HIGH and homed == false) {
                homed = true;
                position = 0;
                numberOfPulses = 0;
            }
            //Move the carriage backwards if not homed
            if (not homed){
                rotateStepper(1, DEFAULT_SPEED, BACKWARDS);
            }
        }
    
    private:
        float mmPerStep = (2 * DEGREES_PER_STEP) / (360 * MICROSTEPPING);
        int carriageDirection = 0;
        int numberOfPulses = 0;
        int pulseWidthMs = (1000000/(2 * DEGREES_PER_STEP * 360 / DEGREES_PER_STEP))/DEFAULT_SPEED;

        //Basic function to rotate the steppermotor
        void rotateStepper(float degrees, int rotationSpeed, bool rotationDirection) {
            numberOfPulses = MICROSTEPPING * degrees/DEGREES_PER_STEP;
            pulseWidthMs = (1000000/(2 * DEGREES_PER_STEP * 360 / DEGREES_PER_STEP))/rotationSpeed;
            digitalWrite(PIN_DIRECTION, rotationDirection);
        }

};

