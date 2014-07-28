/*
  Test.h - Test library for Wiring - implementation
  Copyright (c) 2006 John Doe.  All right reserved.
*/

// include core Wiring API
#include "Arduino.h"

// include this library's description file
#include "EzStepper.h"

// include description files for other libraries used (if any)
#include "HardwareSerial.h"

// Constructor /////////////////////////////////////////////////////////////////
// Function that handles the creation and setup of instances
EzStepper::EzStepper(float gearRatio, float adjustment, int stepPin, int directionPin, byte homePin, byte sleepPin, int slack, int accelerationSpeed, int offset)
{
  // initialize this instance's variables
  _currentPosition = 0;
   _delayDelta = micros();
  _accelerationSpeed = accelerationSpeed;
  
  _gearRatio = gearRatio;
  _adjustment = adjustment;
  _slack = (slack * _gearRatio * _adjustment);
  _offset = (offset * _gearRatio * _adjustment);
  
  _stepPin = stepPin;
  _directionPin = directionPin;
  _currentDirection = 0;
  _homePin = homePin;
  _sleepPin = sleepPin;
  
  _debug = false;
  
  // do whatever is required to initialize the library
  initMotor();

}

// Public Methods //////////////////////////////////////////////////////////////
int EzStepper::getPosition(void)
{
  return _currentPosition;	
}

int EzStepper::getDirection()
{
  return _currentDirection;//digitalRead(_directionPin);
}

void EzStepper::rotateTo(int absolute, int speed) 
{
  long steps = calculateSteps(absolute);
  _currentPosition += steps;
  long totalSteps = abs(steps * _gearRatio * _adjustment);
  if (_debug){Serial.print("CP "); Serial.print(_currentPosition); Serial.print("\n");}
//  setDirection(((steps < 0 ) ? OPEN_DIRECTION : CLOSE_DIRECTION));
  changeDirection(steps);  // Ensures that the head has eaten slack and directon pin is set correctly
  driver(totalSteps, speed);   // Move the head that many steps
  if (_debug){Serial.print("Rotating ");    Serial.print(totalSteps);    Serial.print(" ");    Serial.print(speed);    Serial.print("\n");}
  while (_currentPosition < 0){_currentPosition += 360;}
  while (_currentPosition > 360){_currentPosition -= 360;}
}

void EzStepper::rotateBy(int relative, int speed) 
{
  changeDirection(relative); // Ensures that the head has eaten slack and directon pin is set correctly
  _currentPosition += relative;
  long totalSteps = abs(relative * _gearRatio * _adjustment);
//  setDirection(((relative < 0 ) ? OPEN_DIRECTION : CLOSE_DIRECTION));
  driver(totalSteps, speed);
  while (_currentPosition < 0){_currentPosition += 360;}
  while (_currentPosition > 360){_currentPosition -= 360;}
}


void EzStepper::home(void) {

  int temp = 0;

  int positiveCount = 5; // Number of high's found before considered home
  boolean done = false;
  setDirection(CLOSE_DIRECTION);

  while (!done){
    int stepsTaken = 0;
    int count = 0;  
    while (count < positiveCount){
      
      temp = analogRead(_homePin); 
//        Serial.print("Home: "); Serial.print(temp); Serial.print("\n");
  
      if (temp < 950){
        count = 0;            
      }
      else{
        count = count + 1;  
        if (_debug){
          Serial.print(count);
          Serial.print(" ");
        }
      }  
        long step = abs(1 * _gearRatio * _adjustment); // Move one full step
        stepsTaken = stepsTaken + 1;
        driver(step, _accelerationSpeed);  
    }
    // This case home was not found immediatly so 
    if (_debug){
      Serial.print(stepsTaken);
      Serial.print(" <- Steps taken \n");
    }
    if (stepsTaken > positiveCount){
      done = true;
    }
    // Found home immediatly so re-adjust and try to re-find home.
    else
    {
      if (_debug){Serial.println("Found home immediatly, retrying to find edge");}
      setDirection(OPEN_DIRECTION);
      eatSlack();
      long step = abs((positiveCount * 2) * _gearRatio * _adjustment);
      driver(step, _accelerationSpeed);
      setDirection(CLOSE_DIRECTION);
      eatSlack();
    }
  }
  if (_currentDirection != OPEN_DIRECTION)
  {
      setDirection(OPEN_DIRECTION);
      eatSlack();
  }
  if (_offset != 0){
    eatOffset();
  }
  _currentPosition = 0;
}


// Protected Methods /////////////////////////////////////////////////////////////

void EzStepper::initMotor(void)
{
  // it can also call private functions of this library
  pinMode(_directionPin, OUTPUT);
  pinMode(_stepPin, OUTPUT);
  pinMode(_sleepPin, OUTPUT);
  setDirection(OPEN_DIRECTION);  
}

long EzStepper::calculateSteps(long value)
{
    int temp = value - _currentPosition;
    /* Chooses the fastest way to rotate to the given angle. */
    if (abs(temp) > 180) {
     if (temp < 0){ temp += 360;}
    else {temp -= 360;} 
    }
    if (_debug){Serial.print("Calc Steps "); Serial.print(temp); Serial.print("\n");}
  return temp;//value - _currentPosition;  
}

void EzStepper::eatSlack()
{
    if (_debug){Serial.print("Eliminating Slack.....  ");}
    driver(_slack, _accelerationSpeed);
}

void EzStepper::eatOffset()
{
    changeDirection(_offset);
    if (_debug){Serial.print("Adjusting for offset.....  ");}
    driver(abs(_offset), _accelerationSpeed);
}


void EzStepper::changeDirection(long steps)
{
  // Don't change direction if asked to move to the same location.
  if(steps == 0){return;}
  
    int direction = (steps > 0) ? OPEN_DIRECTION : CLOSE_DIRECTION;
//    Serial.print("changDir "); Serial.print(direction); Serial.print(" curDir "); Serial.print(getDirection()); Serial.print("\n"); 
    // If the direction isn't the same as the direction we last went eat slack
    if (direction != getDirection()){
        setDirection(direction);
        eatSlack();
    }
}

void EzStepper::setDirection(int direction)
{
//    Serial.print("setDir "); Serial.print(direction); Serial.print("\n");
	digitalWrite(_directionPin, direction); 
        _currentDirection = direction;
}

void EzStepper::sleep(boolean sleep_in){
   if (sleep_in){
    digitalWrite(_sleepPin, LOW);
    if (_debug){Serial.println("WRITING LOW");}
   } 
   else{
    digitalWrite(_sleepPin, HIGH); 
    delay(1);
    if (_debug){Serial.println("WRITING HIGH");}
   }
}

void EzStepper::driver(long steps, int speed)
{ 
 unsigned long time, temp;
 time = micros();
//   Serial.print("Driver "); Serial.print(steps); Serial.print(" <- steps | speed -> "); Serial.println(speed);  
   long delay;
  for (long i = 0; i < steps; i++) {

    _delayDelta = micros() - _delayDelta;
     delay = ((long)speed - _delayDelta/2) - DELAY_COMPENSATION;
  //  temp = micros();
//    if (delay < MINIMUM_DELAY)
//    {
//      delay = MINIMUM_DELAY;
//    }

//    byte pin = A10;
//    int delay = map(analogRead(pin), 0, 1023, 230, 2100);
//    Serial.println(delay);
    digitalWrite(_stepPin, OPEN_DIRECTION);
    delayMicroseconds(delay);
//        Serial.print(delay);Serial.print(" One delay "); Serial.println(micros() - temp);
    
    digitalWrite(_stepPin, CLOSE_DIRECTION);
    delayMicroseconds(delay);
    //Serial.print("Two delay "); Serial.println(micros() - temp);
     _delayDelta = micros();
  }
//    Serial.print("<- delayDelta | delay -> "); Serial.println(delay);
//  time = micros() - time;
//  Serial.println(time);
}

void EzStepper::acceleration(int absolute, int speed){
  
  long steps = calculateSteps(absolute);
  _currentPosition += steps;
  long totalSteps = abs(steps * _gearRatio * _adjustment);
  Serial.print("CP "); Serial.print(_currentPosition); Serial.print("\n");
//  setDirection(((steps < 0 ) ? OPEN_DIRECTION : CLOSE_DIRECTION));
  changeDirection(steps);  // Ensures that the head has eaten slack and directon pin is set correctly

  long ACCELERATION = 1;
  int MIN_DELAY = speed;
  int STARTING_SPEED = 700;
  long RAMP_STEPS = (STARTING_SPEED-MIN_DELAY)/ACCELERATION;
  int stepsTaken  = 0;
  int delay = STARTING_SPEED;
  
  if (totalSteps > (RAMP_STEPS * 2)){
    while (stepsTaken < RAMP_STEPS){
      driver(1, delay);
      delay -= ACCELERATION;
      stepsTaken++;
    }
    delay = MIN_DELAY;
    driver((totalSteps - 2 * RAMP_STEPS), delay);
    stepsTaken += (totalSteps - 2 * RAMP_STEPS);
    
    while (stepsTaken < totalSteps) {
      driver(1, delay);
      delay += ACCELERATION;
      stepsTaken++;
    }
  }
  else{
    while(stepsTaken <= totalSteps /2){
      driver(1, delay);
      delay -= ACCELERATION;
      stepsTaken++;
    }
    delay += ACCELERATION;
    while (stepsTaken < totalSteps) {
      driver(1, delay);
      delay += ACCELERATION;
      stepsTaken++;
    }
  }
  Serial.print("Rotating ");    Serial.print(totalSteps);    Serial.print(" ");    Serial.print(speed);    Serial.print("\n");
}

void EzStepper::setdebug(boolean in){
   _debug = in;
}


