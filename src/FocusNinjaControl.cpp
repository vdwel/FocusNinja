#include "FocusNinjaControl.h"
#include <Arduino.h>

FocusNinjaControl::FocusNinjaControl()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    pinMode(PIN_DIRECTION, OUTPUT);
    pinMode(PIN_STEP, OUTPUT);
    pinMode(PIN_CAMERA_RELEASE, OUTPUT);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_CAMERA_RELEASE, LOW);
}

void FocusNinjaControl::takePhotos(float startPos, float endPos, int steps)
{
    beginPosition = startPos / MILLIMETER_PER_STEP;
    endPosition = endPos / MILLIMETER_PER_STEP;
    remainingSteps = steps;
    // first go to the start position
    if (homed)
    {
        destinationPosition = beginPosition;

        fixDestination();

        if (destinationPosition > position)
        {
            setDirection(1);
        }
        else
        {
            setDirection(-1);
        }

        setState(MOVE_TO_SHOT);
    }
    else
    {
        setDirection(-1);
        setState(HOME_TO_START);
    }
}

void FocusNinjaControl::moveCarriage(float millimeter, bool direction)
{

    if (direction == FORWARDS)
    {
        setDirection(1);
        destinationPosition += millimeter / MILLIMETER_PER_STEP;
    }
    else
    {
        setDirection(-1);
        destinationPosition -= millimeter / MILLIMETER_PER_STEP;
    }

    fixDestination();

    state = MOVE_TO_IDLE;
}

//Home the carriage
void FocusNinjaControl::homeCarriage()
{
    setDirection(-1);
    setState(HOME_TO_IDLE);
    log("log Homing.");
}

bool FocusNinjaControl::isMoving()
{
    return IDLE != state;
}

bool FocusNinjaControl::isHome()
{
    return LOW == digitalRead(PIN_ENDSTOP);
}

bool FocusNinjaControl::isDestinationReached()
{
    return (carriageDirection > 0 && position >= destinationPosition) ||
           (carriageDirection < 0 && position <= destinationPosition) ||
           (carriageDirection == 0);
}

void FocusNinjaControl::pressShutter()
{
    log("log Shutter pressed.");
    digitalWrite(PIN_CAMERA_RELEASE, HIGH);
}

void FocusNinjaControl::releaseShutter()
{
    log("log Shutter released.");
    digitalWrite(PIN_CAMERA_RELEASE, LOW);
}

void FocusNinjaControl::stateMachine()
{
    switch (state)
    {
    case IDLE:
        // nothing to do, remain in state
        destinationPosition = position;
        break;
    case HOME_TO_IDLE:
        if (isHome())
        {
            log("log Homed.");
            carriageDirection = 0;
            position = 0;
            homed = true;
            reportPosition();
            setState(IDLE);
        }
        else
        {
            step();
        }
        break;
    case HOME_TO_START:
        if (isHome())
        {
            // move to begin
            homed = true;
            position = 0;
            destinationPosition = beginPosition;
            fixDestination();
            setDirection(1);
            reportPosition();
            setState(MOVE_TO_SHOT);
        }
        else
        {
            step();
        }

        break;
    case MOVE_TO_IDLE:
        if (isDestinationReached())
        {
            carriageDirection = 0;
            reportPosition();
            setState(IDLE);
        }
        else
        {
            step();
        }
        break;
    case MOVE_TO_SHOT:
        if (position == destinationPosition)
        {
            // destination reached
            carriageDirection = 0;
            timeout_start = millis();
            timeout_wait = shutterDelay;
            reportPosition();
            setState(WAIT_BEFORE_SHUTTER);
        }
        else
        {
            step();
        }
        break;
    case WAIT_BEFORE_SHUTTER:
        if (millis() - timeout_start > timeout_wait)
        {
            this->pressShutter();
            timeout_start = millis();
            timeout_wait = triggerTime;
            setState(SHUTTER_DOWN);
        }
        break;

    case SHUTTER_DOWN:
        if (millis() - timeout_start > timeout_wait)
        {
            this->releaseShutter();
            timeout_start = millis();
            timeout_wait = shutterAfterDelay;
            setState(WAIT_AFTER_SHUTTER);
        }
        break;

    case WAIT_AFTER_SHUTTER:
        if (millis() - timeout_start > timeout_wait)
        {
            // move to next positions
            if (remainingSteps > 0)
            {
                setDirection(1);
                destinationPosition += (endPosition - position) / remainingSteps;
                fixDestination();
                setState(MOVE_TO_SHOT);
                remainingSteps -= 1;
            }
            else
            {
                setState(IDLE);
            }
        }
        break;

    default:
        break;
    }
}

void FocusNinjaControl::setState(FocusState s)
{
    Serial.printf("Set state to %d\r\n", s);
    state = s;
}

void FocusNinjaControl::fixDestination(void)
{
    // generic check
    if (homed)
    // if not homed, positions do not make sense
    {
        if (destinationPosition > MAX_POSITION)
        {
            destinationPosition = MAX_POSITION;
        }
        else if (destinationPosition < 0)
        {
            destinationPosition = 0;
        }
    }
}

void FocusNinjaControl::stop()
{
    carriageDirection = 0;
    setState(IDLE);
    log("log Stopped.");
}

void FocusNinjaControl::setDirection(int direction)
{
    carriageDirection = direction;
    Serial.printf("Direction %d\r\n", direction);
    digitalWrite(PIN_DIRECTION, -1 == carriageDirection);
}

void FocusNinjaControl::step()
{
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(pulseWidthMs);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(pulseWidthMs);
    position += carriageDirection;
    if (millis() - lastStepReport > STEP_REPORT_INTERVAL_MILLIS)
    {
        lastStepReport = millis();
        Serial.printf("Position %d, stepped %d\r\n", position, carriageDirection);
        reportPosition();
    }
}

void FocusNinjaControl::setLogger(void (*fun_ptr)(const char *))
{
    logger = fun_ptr;
}

void FocusNinjaControl::log(const char *s)
{
    Serial.printf("Log: %s\r\n", s);

    if (logger)
    {
        logger(s);
    }
}

void FocusNinjaControl::reportPosition()
{
    char buf[32];

    snprintf(buf, 32, "pos %f", position * MILLIMETER_PER_STEP);
    log(buf);
}