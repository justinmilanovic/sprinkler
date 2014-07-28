

// ensure this library description is only included once
#ifndef DU_h
#define DU_h

// include types & constants of Wiring core API
#include "Arduino.h"


class DU
{
  
  public:
    DU();
    DU(int max_distance, int increment[]);
    int update(int distance, int lap);
    int speed(int distance);

  protected:
    float _max_distance;
    int* _increment;
			
};
#endif

