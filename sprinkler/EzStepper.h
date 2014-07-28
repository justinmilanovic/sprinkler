/*
  Test.h - Test library for Wiring - description
  Copyright (c) 2006 John Doe.  All right reserved.
*/

// ensure this library description is only included once
#ifndef EzStepper_h
#define EzStepper_h
#define OPEN_DIRECTION LOW
#define CLOSE_DIRECTION HIGH
#define DELAY_COMPENSATION 15

// include types & constants of Wiring core API
#include "Arduino.h"


// library interface description
class EzStepper
{
  // user-accessible "public" interface
  public:

    //EzStepper(void);  
    EzStepper(float gearRatio=0.0, float adjustment=1.0, int stepPin=0, int directionPin=0, byte homePin='a0', byte sleepPin='NO', int slack=0, int accelerationSpeed=0, int offset = 0);
        
	int getPosition(void);
	int getDirection(void);
        static const long MINIMUM_DELAY = 70ul;
	void rotateBy(int relative, int speed);
	void rotateTo(int absolute, int speed);
	void home(void);
        void acceleration(int absolute, int speed);
        void sleep(boolean sleep);
        void setdebug(boolean in);
        
        boolean _debug;

  // library-accessible "private" interface
  protected:
	
	int _currentPosition;
	int _accelerationSpeed;
        unsigned long _delayDelta;
	
	float _gearRatio;
	float _adjustment;
	int _slack;
        int _offset;
	
	int _stepPin;
	int _directionPin;
        int _currentDirection;
	byte _homePin;
        byte _sleepPin;
        
	void initMotor(void);
	long calculateSteps(long value);
        void eatSlack();
        void eatOffset();
	void changeDirection(long steps);
	void setDirection(int direction);
	void driver(long steps, int speed);
	};

#endif



