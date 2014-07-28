

// include core Wiring API
#include "DU.h"

// include this library's description file
#include "DU.h"

// include description files for other libraries used (if any)
#include "HardwareSerial.h"


const int SPEED[2][5] =
{
{ 100, 110, 120, 130, 140, }, // row 0
{ 325, 525, 700, 750, 810, }, // row 1
};

DU::DU(void){

}

DU::DU(int max_distance, int increment[])
{
  _max_distance = max_distance;
  _increment = increment;
}

// Public Methods 

int DU::speed(int distance)
{
  if(distance < SPEED[0][0]){return SPEED[1][0];}
  if(distance > SPEED[0][4]){return SPEED[1][4];}
  
  //find index of distance in speed
  int lower_bound = 0;
  for(int i =0; distance > SPEED[0][i]; i++){
    lower_bound = i;
  }
  int upper_bound = lower_bound +1;

  //
  int offset = distance - SPEED[0][lower_bound];
  float percent = offset/10.0;
  int range = SPEED[1][upper_bound] - SPEED[1][lower_bound];
  int adjusted_speed = SPEED[1][lower_bound] + (range*percent);
  return adjusted_speed;
}

int DU::update(int distance, int lap)
{
  float x = distance/_max_distance;
  float flow = distance - ((x*x) * _increment[lap]);
  int rounded_flow = ceil(flow-0.5);
  return rounded_flow;
}

// Private Methods   
  
  	


