#ifndef FOCUSNINJACONTROL_H
#define FOCUSNINJACONTROL_H

#include <Arduino.h>

#define PIN_DIRECTION 19
#define PIN_STEP 18
#define PIN_ENDSTOP 32
#define PIN_CAMERA_RELEASE 21
#define PIN_STATUS_LED 5

#define MICROSTEPPING 4
#define DEGREES_PER_STEP 1.8
#define MILLIMETER_PER_ROTATION 2
#define MILLIMETER_PER_STEP (MILLIMETER_PER_ROTATION * DEGREES_PER_STEP / (MICROSTEPPING * 360))
#define DEFAULT_SPEED 2                                      // rotations per second
#define MAX_POSITION_MM 130                                  // max position in mm
#define MAX_POSITION (MAX_POSITION_MM / MILLIMETER_PER_STEP) // max position in steps
#define STEP_REPORT_INTERVAL_MILLIS 500                      // milliseconds between reports of position while moving
#define FORWARDS false
#define BACKWARDS true

class FocusNinjaControl
{
public:
    bool homed = false;
    int position = -1;     // in steps
    int beginPosition = 0; // where to start taking shots, in steps
    int endPosition = 0;   // where to stop taking shots, in steps
    float jogSize = 1;
    int numberOfSteps = 0;
    int stepCount = 0;
    int shutterDelay = 2000;
    int shutterAfterDelay = 1000;
    int triggerTime = 80;

    //Initialization code
    FocusNinjaControl();
    void moveCarriage(float millimeter, bool direction);
    //Home the carriage
    void homeCarriage();
    bool isMoving();
    bool isHome();
    bool isDestinationReached();
    void stop();
    void pressShutter();
    void releaseShutter();
    void takePhotos(float startPos, float endPos, int steps);
    void stateMachine();
    //This needs to be called from the loop
    void motorControl();
    void setLogger(void (*fun_ptr)(const char *));
    void log(const char *);
    void reportPosition();

private:
    enum FocusState
    {
        IDLE,                // nothing to do, not moving
        HOME_TO_IDLE,        // determine where home is, to idle when done
        HOME_TO_START,       // determine where home is, to begin when done
        MOVE_TO_IDLE,        // move to position, to idle when done
        MOVE_TO_SHOT,        // move to beginning, start taking pictures after
        WAIT_BEFORE_SHUTTER, // waiting in position before shutter
        SHUTTER_DOWN,        // shutter is down, timeout after
        WAIT_AFTER_SHUTTER   // waiting after shutter up
    };

    FocusState state = IDLE;
    int carriageDirection = 0;
    int numberOfPulses = 0;
    int pulseWidthMs = 1000000 / ((360 / DEGREES_PER_STEP) * MICROSTEPPING * 2) / DEFAULT_SPEED;
    void (*logger)(const char *) = 0;
    int timeout_start = 0;
    int timeout_wait = 0;
    int destinationPosition = 0;
    int remainingSteps = 0;
    int lastStepReport = 0;

    void setDirection(int direction);
    void fixDestination(void);
    void setState(FocusState s);

    //Basic function to rotate the steppermotor
    void step();
};
#endif