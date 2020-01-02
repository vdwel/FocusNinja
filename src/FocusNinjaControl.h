#ifndef FOCUSNINJACONTROL_H
#define FOCUSNINJACONTROL_H

#define PIN_DIRECTION 19
#define PIN_STEP 18
#define PIN_ENDSTOP 32
#define PIN_CAMERA_RELEASE 21

#define MICROSTEPPING 4
#define DEGREES_PER_STEP 1.8
#define MILLIMETER_PER_ROTATION 2
#define DEFAULT_SPEED 2  //omwentelingen per seconde
#define MAX_POSITION 130 //maximale positie in mm
#define FORWARDS false
#define BACKWARDS true

class FocusNinjaControl
{
public:
    bool homed = false;
    float position = 0;

    //Initialization code
    FocusNinjaControl();
    void moveCarriage(float millimeter, bool direction);
    //Home the carriage
    void homeCarriage();
    bool isMoving();
    void stop();
    void releaseShutter();
    //This needs to be called from the loop
    void motorControl();
    void setLogger(void (*fun_ptr)(const char*));
    void log(const char*);

private:
    float mmPerStep = (2 * DEGREES_PER_STEP) / (360 * MICROSTEPPING);
    int carriageDirection = 0;
    int numberOfPulses = 0;
    int pulseWidthMs = (1000000 / (2 * DEGREES_PER_STEP * 360 / DEGREES_PER_STEP)) / DEFAULT_SPEED;
    void (*logger)(const char*) = 0;

    //Basic function to rotate the steppermotor
    void rotateStepper(float degrees, int rotationSpeed, bool rotationDirection);
    void reportPosition(float pos);
};
#endif