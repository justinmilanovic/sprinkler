

#include <EEPROM.h>
#include <string.h>
#include "EzStepper.h"
#include "DU.h"

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// GLOBALS
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//zones
#define MAX_COMMAND_LENGTH 40
#define MAX_ANGLE 360
const int NUMBER_OF_ZONES = 3;
int ZONE_ARRAYS[NUMBER_OF_ZONES][MAX_ANGLE] = {0};
EzStepper SPRINKLER[NUMBER_OF_ZONES] = {};
int CURRENT_ZONE = 0;

//input+output
/*** OLD ARDUINO PINS ***/
/*
#define FLOW_DIRECTION_PIN 10
#define FLOW_STEP_PIN 11
const byte FLOWSWITCH = A8;  // Flow valve home sensor
const byte FLOW_SLEEP = A10; // Enable or disable driver
const byte HOME_SWITCHES[NUMBER_OF_ZONES] = {A9, A9, A9};
const byte SPRINKLER_SLEEPS[NUMBER_OF_ZONES] = {A11, A11, A11};
const int DIRECTION_PINS[NUMBER_OF_ZONES] = {8, 8, 8};
const int STEP_PINS[NUMBER_OF_ZONES] = {9, 9, 9};
const int ZONE_INPUT_PORTS[NUMBER_OF_ZONES] = {22, 24, 26};//, 26, 28, 30, 32};
*/
/*** NEW ARDUINO PINS ***/
#define FLOW_DIRECTION_PIN 32
#define FLOW_STEP_PIN 30
const byte FLOWSWITCH = A9;  // Flow valve home sensor
const byte FLOW_SLEEP = 28; // Enable or disable driver
const byte HOME_SWITCHES[NUMBER_OF_ZONES] = {A10, A10};
const byte SPRINKLER_SLEEPS[NUMBER_OF_ZONES] = {22, 22};
const int DIRECTION_PINS[NUMBER_OF_ZONES] = {26, 26};
const int STEP_PINS[NUMBER_OF_ZONES] = {24, 24};
const int ZONE_INPUT_PORTS[NUMBER_OF_ZONES] = {10, 12};//, 26, 28, 30, 32};


//stepper motor - flow
const float flowGearRatio = 100.00; 
float gearRatio = 32.5;//100.00;//26.85;      // 25.25 in documentation

// Pattern storage
int pattern_index = 0;
enum PatternAttributes{
  PATTERN_START_ANGLE_OFFSET = 0,
  PATTERN_END_ANGLE_OFFSET,
  PATTERN_FLOW_OFFSET,
  // PATTERN_DIRECTION_OFFSET, // May not be needed anymore. The Sprinkler now rotates the shortest distance to it's desired angle
  // PATTERN_REPEAT_OFFSET, // Remove after figuring out the merging of patterns
  // ADD NEW ATTRIBUTES HERE
  NUMBER_OF_PATTERN_ATTRIBUTES // This should always be last
};
#define MAX_NUMBER_OF_PATTERN_STEPS MAX_ANGLE  // Max number of full attributes that can be stored
// Needed since multiplication doesn't seem to work in array declaration
#define PATTERN_ARRAY_SIZE (MAX_NUMBER_OF_PATTERN_STEPS*NUMBER_OF_PATTERN_ATTRIBUTES)
int PATTERN[PATTERN_ARRAY_SIZE] = {0};
//float PATTERN_SPEED_ARRAY[MAX_NUMBER_OF_PATTERN_STEPS] = {0};  // Seperate array since speed may be float

/* Algorithm Values*/
#define NUMBER_OF_SWEEPS 12  // The number of passes being made
//{1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
int INTERVALS[NUMBER_OF_SWEEPS] = { 0,0,1,1,2,4,7,11,16,22,29,37 }; //This array defines how much is pulled back each pass
const int MAX_FLOW_RATE = 220;
DU ALGORITHM;

/* Shared Values */
const int MAX_MOTOR_SPEED = 100; // The delay between each step in micro seconds
const int MIN_MOTOR_SPEED = 32000; // The delay between each step in micro seconds
const int MAXCOUNT = 5; // Number of highs recorded before it registers home sensor
const int MAXVOLTAGE = 950; // Limit for reading a high from the home sensor.
const float KEYLARGE  = 5;//10; //Amount the adjustment commands move
const float KEYSMALL  = 1;//5;
const boolean AWAKE = false; // If to send a high or low to awake the driver. true = Low
const boolean SLEEP = true; // If to send a high or low to sleep the driver.  true = Low

/* Sprinkler Values  */
EzStepper sprinklerStepper; // Contains reference to the current selected zones sprinkler
const int DEFAULT_SPRINKLER_ROTATE_SPEED = 700; // Default speed for rotating the sprinkler head
const int SLACK = 0; // Slack value for the sprinkler head.
const float SPRINKLERMULTIPLIER = 3200/360; // 1.8/360 * 16 = 3200 steps per full rotation. This is to adjust so that one 'step' = 1 degree
//const int SLACK = 1;
//int lapCount = 0;

/* Flow Variables */
EzStepper flowStepper; // Contains reference to the flow control valve
const int DEFAULT_FLOW_ROTATE_SPEED = MAX_MOTOR_SPEED; // Default rotations for flow control valve
const float FLOWMULTIPLIER = 2; // This is to adjust so that one 'step' is still a reasonable movement
const int FLOWSLACK = 3;  // Slack value set for the flow control valve
const int FLOW_OFFSET = -20; // Adjustment on the valve after it has found the home sensor to adjust to a closed state.

/* Control variables */
boolean running = false;  // Used as a control variable for command reading
boolean protocol = true;  // Used as a control vairable for command reading. Mostly relevent if the PI is used
boolean rasp_pi = false;

const char CR = '\r';      // carriage return
const int VERSION_NUMBER = 11; //the version of the code release after TRLabs had begun working on it

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// INITIALIZE MOTORS
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/* Initializes all the sprinkler stepper values for each zone */
void initMotor() {
  for (int i = 0; i < NUMBER_OF_ZONES; i++)
  {
    SPRINKLER[i] = EzStepper(gearRatio, SPRINKLERMULTIPLIER, STEP_PINS[i], DIRECTION_PINS[i], HOME_SWITCHES[i], SPRINKLER_SLEEPS[i], SLACK, MAX_MOTOR_SPEED);
    SPRINKLER[i].sleep(SLEEP);
  }
  sprinklerStepper = SPRINKLER[CURRENT_ZONE];//EzStepper(gearRatio, SPRINKLERMULTIPLIER, STEP_PIN, DIRECTION_PIN, HOMESWITCH, SPRINKLER_SLEEP, SLACK, MAX_MOTOR_SPEED);
  Serial.println("Initializing Sprinkler Motor.....");
}

/* Initializes the flow control motor */
void initFlowMotor() {
  flowStepper = EzStepper(flowGearRatio, FLOWMULTIPLIER, FLOW_STEP_PIN, FLOW_DIRECTION_PIN, FLOWSWITCH, FLOW_SLEEP, FLOWSLACK, MAX_MOTOR_SPEED, FLOW_OFFSET);
  flowStepper.sleep(SLEEP);
  Serial.println("Initializing Flow Motor.....");
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// ROTATE
// sprinklerStepper refers to the currently active zone's head.
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/* Rotates the sprinkler head the given number of degrees. */
void rotate(int value) {
  rotate(value, DEFAULT_SPRINKLER_ROTATE_SPEED);
}
/* Rotates the sprinkler head the given number of degrees at the given speed. */
void rotate(int value, int speed)
{
  // Uncomment to limit speed used
  //  if (speed > MIN_MOTOR_SPEED)  // Note: Min Motor speed is a high number
  //      { Serial.print("SETTING MIN SPEED");speed = MIN_MOTOR_SPEED;}
  //  else if (speed < MAX_MOTOR_SPEED) // Note: max motor speed is a low number
  //      {Serial.print("SETTING MAX SPEED"); speed = MAX_MOTOR_SPEED; }
  if (!protocol){
    Serial.print("Sprinkler Rotating to ");
    Serial.println(value);
  }
  sprinklerStepper.sleep(AWAKE);
  sprinklerStepper.rotateTo(value, speed);
  sprinklerStepper.sleep(SLEEP);
}
/* Sends the sprinkler to the home sensor */
void home() {
  if (!protocol){
    Serial.print("Seeking Home......  ");
  }
  sprinklerStepper.sleep(AWAKE);
  sprinklerStepper.home();
  sprinklerStepper.sleep(SLEEP);
  if (!protocol){
    Serial.println("......Found Home ");
  }
}

/* Get the current angle of the sprinkler head */
float getAngle() {
  return sprinklerStepper.getPosition();
}


// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// FLOW 
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/* Rotates the flow valve the given number of 'steps'. */
void flow(int flowValue) {
  flow(flowValue, DEFAULT_FLOW_ROTATE_SPEED);
}
/* Rotates the flow valve the given number of 'steps' at the given speed. */
void flow(int value, int speed)
{
  // Uncomment to limit the speed
  //  if (speed > MIN_MOTOR_SPEED)  // Note: Min Motor speed is a high number
  //      { Serial.print("SETTING MIN SPEED");speed = MIN_MOTOR_SPEED;}
  //  else if (speed < MAX_MOTOR_SPEED) // Note: max motor speed is a low number
  //      {Serial.print("SETTING MAX SPEED"); speed = MAX_MOTOR_SPEED; }
  if (!protocol){
    Serial.print("Flow Valve Rotating to ");
    Serial.println(value);
  }
  flowStepper.sleep(AWAKE);
  flowStepper.rotateTo(value, speed);
  flowStepper.sleep(SLEEP);
}
/* Sends the flow control valve to the home sensor */
void shutflow(){ 
  if (!protocol){
    Serial.print("Seeking Shutoff......  ");
  }
  flowStepper.sleep(AWAKE);
  flowStepper.home();
  flowStepper.sleep(SLEEP);
  if (!protocol){
    Serial.println("......Found  "); 
  }
}

/* GET CURRENT FLOW VALUE */
float getFlow() {
  return flowStepper.getPosition();
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// SETUP
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/* Initializes motors, sets baud rate, reads the saves patterns from memory */
void setup() {
  Serial.begin(115200); // initialize serial communication, was 57600
  populateZoneArraysFromMemory();// Read the values for flow rates from EEPROM. (possibly replace with storing pattern)
  ALGORITHM = DU(MAX_FLOW_RATE, INTERVALS); 
  initMotor();
  initFlowMotor();
  Serial.print("Initializing Version No: ");
  Serial.print(VERSION_NUMBER, DEC);
  Serial.println(" .....Ready");
  
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// FUNCTIONS RELEVENT TO PATTERNS / RUN TIME
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/* Used to store pattern into the pattern array. - Was used by PI, currently not used */
/*
void storeSprayPattern(int start_angle, int end_angle, int direc, int flowRate, float rot_speed, int repeat){
  // Store the values in the pattern array
  Serial.println("Storing Pattern");  
  if (pattern_index >= MAX_NUMBER_OF_PATTERN_STEPS)
  {
    return; 
  }

  int start_index = pattern_index * NUMBER_OF_PATTERN_ATTRIBUTES;

  PATTERN[start_index + PATTERN_START_ANGLE_OFFSET] = start_angle;
  PATTERN[start_index + PATTERN_END_ANGLE_OFFSET] = end_angle;
  //PATTERN[start_index + PATTERN_DIRECTION_OFFSET] = direc;
  //PATTERN[start_index + PATTERN_REPEAT_OFFSET] = repeat;
  PATTERN[start_index + PATTERN_FLOW_OFFSET] = flowRate;
  //PATTERN_SPEED_ARRAY[pattern_index] = rot_speed * DEFAULT_SPRINKLER_ROTATE_SPEED;

  pattern_index = pattern_index + 1;

  Serial.print("Added: Start angle: "); 
  Serial.print(start_angle);
  Serial.print(", End angle: ");  
  Serial.print(end_angle);
  Serial.print(", Flow Rate: "); 
  Serial.print(flowRate);
  Serial.print(", Repeat: ");  
  Serial.print(repeat);
  Serial.print(", Speed: ");  
  Serial.println(rot_speed * DEFAULT_SPRINKLER_ROTATE_SPEED);

}
*/

/* Clears the pattern that is currently being saved in the pattern array - was used by PI, currently not being used.*/
/*void clearPattern(){
  Serial.println("Clearing Pattern");
  pattern_index = 0;  // Note this does not clear the array of the previous pattern, but just resets the indexing 
}*/

/* Prints the pattern currently stored in the pattern array in a human readable format */
void printPattern(){
  setupPatternFromZone();
  Serial.print("Pattern Index: ");
  Serial.println(pattern_index);
  for (int index = 0; index < pattern_index; index++)
  {
    int base_ind = index * NUMBER_OF_PATTERN_ATTRIBUTES;
    int start_angle = PATTERN[base_ind + PATTERN_START_ANGLE_OFFSET];    
    int end_angle = PATTERN[base_ind + PATTERN_END_ANGLE_OFFSET];    
    int flowRate = PATTERN[base_ind + PATTERN_FLOW_OFFSET];
    //int repeat = PATTERN[base_ind + PATTERN_REPEAT_OFFSET];
    //int rot_speed = PATTERN_SPEED_ARRAY[index];

    Serial.print("Start angle: "); 
    Serial.print(start_angle);
    Serial.print(", End angle: "); 
    Serial.print(end_angle);
    Serial.print(", Flow Rate: "); 
    Serial.println(flowRate);
  }
}
/* Runs the currently stored pattern in the pattern array. - was used by the PI currently not being used.
** CURRENTLY OUTDATE WITH HOW WE ARE USING THE PATTERN ARRAY NOW 
*/
/*
void runStoredPattern(){
  Serial.println("Running Pattern");
  int start_angle, end_angle, dir, flowRate, repeat, base_ind;
  float rot_speed;
  for (int index = 0; index < pattern_index; index++){
    base_ind = index * NUMBER_OF_PATTERN_ATTRIBUTES;

    start_angle = PATTERN[base_ind + PATTERN_START_ANGLE_OFFSET];    
    end_angle = PATTERN[base_ind + PATTERN_END_ANGLE_OFFSET];    
    //    dir = PATTERN[base_ind + PATTERN_DIRECTION_OFFSET];    
    flowRate = PATTERN[base_ind + PATTERN_FLOW_OFFSET];
    //repeat = PATTERN[base_ind + PATTERN_REPEAT_OFFSET];
    //rot_speed = PATTERN_SPEED_ARRAY[index];
    // Set speed
    if (rot_speed > MIN_MOTOR_SPEED)  // Note: Min Motor speed is a high number
    { 
      rot_speed = MIN_MOTOR_SPEED;
    }
    else if (rot_speed < MAX_MOTOR_SPEED) // Note: max motor speed is a low number
    {
      rot_speed = MAX_MOTOR_SPEED; 
    }

    Serial.print("Angles: ");
    Serial.print(start_angle);
    Serial.print(" ");
    Serial.println(end_angle);   

    Serial.print("lap total: ");
    Serial.println(repeat); 
    // Repeat the current pattern for the set amount of times.
    rotate(start_angle, MAX_MOTOR_SPEED);
    flow(flowRate);

    unsigned long time;
    time = micros();

    while(lapCount < repeat){
      rotate(end_angle, rot_speed);
      int temp = start_angle;
      start_angle = end_angle;
      end_angle = temp;
      lapCount++;
    } // End While 
    lapCount = 0;
    time = micros() - time;
    Serial.println(time);

  } // End For
  shutflow();
  home();
}*/

/* Sets the flow values to zero for the current zone */
void zeroCurrentZone(){
  Serial.print("Zeroing zone "); Serial.println(CURRENT_ZONE);
  for (int i = 0; i < MAX_ANGLE; i++)
  {
    updateZoneArray(CURRENT_ZONE, 0, i);
  }
}

/* Saves the current flow rate to the current sprinkler angle 
  and then rotates in the positive direction by 1 */
void saveAndMove(){
  int curPos = sprinklerStepper.getPosition();  // Get current angle
  int flowRate = flowStepper.getPosition();    // Get current flow
  rotate(curPos+1);  // Increment the angle by one
  updateZoneArray(CURRENT_ZONE, flowRate, curPos); // Save position into array
}

/* Changes the entire array from the first angle to the second angle to the given flowrate for the Current Zone */
void changeArray(int firstAngle, int lastAngle, int flowRate){
  if (!protocol){Serial.print("Calling Change Array for zone "); Serial.println(CURRENT_ZONE);}
// Make sure the angle is valid
  if ((0 <= firstAngle) && (0 <= lastAngle) 
        && (firstAngle <= lastAngle)
        && (firstAngle <= MAX_ANGLE) &&(lastAngle <= MAX_ANGLE)
     )
  {
  if (!protocol){ Serial.print("Adding in: "); Serial.print(firstAngle); Serial.print(" - "); Serial.print(lastAngle); 
  Serial.print(" at "); Serial.println(flowRate);}
    for (int i = firstAngle; i < lastAngle; i++)
    {
      updateZoneArray(CURRENT_ZONE, flowRate, i);
    }
  }
  else{  Serial.println("Invalid angles, accepts 0 - 360"); }
}

/* Print current zone array */
void printArray(){
 setupPatternFromZone();
 printPattern();
   for (int angle = 0; angle < MAX_ANGLE; angle++){
    Serial.println(ZONE_ARRAYS[CURRENT_ZONE][angle]); 
   }
}

/* Handles the writing to physical memory and updating RAM array */
void updateZoneArray(int zone, int flowRate, int angle){
  if ((0 <= angle) && (angle <= MAX_ANGLE) && (zone < NUMBER_OF_ZONES))
   {
   ZONE_ARRAYS[zone][angle] = flowRate;
   EEPROM.write(MAX_ANGLE*zone + angle, flowRate); 
   }
}

/* Read all the zone information from memory and places it in the zone arrays */
void populateZoneArraysFromMemory(){
  for(int zone = 0; zone < NUMBER_OF_ZONES; zone++){
   for (int i = 0; i < MAX_ANGLE; i++)
   {
     int flowRate = EEPROM.read(zone*MAX_ANGLE + i);
     if (flowRate == 255){flowRate = 0;}
     ZONE_ARRAYS[zone][i] = flowRate;
   }}
}

/* Populates the pattern array from the current zone array 
// Starts the pattern at the beginning of the last contious section defined. This allows for patterns that go 
// across the home sensor. */
void setupPatternFromZone(){
  int start_angle, end_angle, flowRate, current_val;
  pattern_index = 0; // Resets the previous pattern
  int starting_index = 0; // starting index for ordering pattern
  int temp = 0;
  /* Find the starting angle. First non-zero value of the highest continous range */
  for (int angle = 0; angle < MAX_ANGLE; angle++){
    flowRate = ZONE_ARRAYS[CURRENT_ZONE][angle];
    if (flowRate > 0 and temp == 0){starting_index = angle; temp=flowRate;}
    if (flowRate == 0){temp = 0;}
  }
  if(debug){Serial.print("Starting INdex: "); Serial.println(starting_index);}
  
  /* Iterate though all the angles, though not necessarily starting at 0*/
  for (int count = 0; count < MAX_ANGLE; count++){
    int angle = (starting_index + count) % (MAX_ANGLE); // Figure out current angle
    flowRate = ZONE_ARRAYS[CURRENT_ZONE][angle];
    if (flowRate > 0){  // None zero flow rate found, so adding new pattern
      start_angle = angle;
      current_val = ZONE_ARRAYS[CURRENT_ZONE][angle];
      // Count up the angles until we hit the max angle. 
      for (int nex_ang = angle +1; nex_ang <= MAX_ANGLE; nex_ang++){
        /* If the next angle is one of the following create a new patter:
        // 1) Equal to the max angle, 2) The flow rate on the next angle is different 3) The current line goes past half the max angle
        // The third requirment is there to ensure that the sprinkler head rotates the proper way. (Since it rotates the shortest distances) */
        if (nex_ang == MAX_ANGLE || ZONE_ARRAYS[CURRENT_ZONE][nex_ang] != current_val || nex_ang - angle == MAX_ANGLE/2){
          end_angle = nex_ang; 
          count += nex_ang - angle - 1; // Subtract one since it will be incremented at the end of this
          break;
        }
      }
      /* To stop buffer overflow */
      if (pattern_index >= MAX_NUMBER_OF_PATTERN_STEPS)
      {
        if(debug){Serial.print("Pattern Array full");}
        return; 
      }
      /* Inserts the new section into the pattern array */
      int start_index = pattern_index * NUMBER_OF_PATTERN_ATTRIBUTES;
      PATTERN[start_index + PATTERN_START_ANGLE_OFFSET] = start_angle;
      PATTERN[start_index + PATTERN_END_ANGLE_OFFSET] = end_angle;
      PATTERN[start_index + PATTERN_FLOW_OFFSET] = flowRate;
      pattern_index = pattern_index + 1;
    }
  }
}

/* Runs the current zone. If runOnce is true it will only spray the first pass. */
void runZone(boolean runOnce = false){
  if (!protocol){Serial.print("Running Zone ");   Serial.println(CURRENT_ZONE);}
  int start_angle, end_angle, flowRate, base_ind;

  setupPatternFromZone(); // Repopulates the pattern array

  int index = 0;
  int cur_dir = 1;
  int sweep_count = 0;
  while( runOnce || sweep_count < NUMBER_OF_SWEEPS) //digitalRead(ZONE_INPUT_PORTS[CURRENT_ZONE]) > 0 ||
  {
    base_ind = index * NUMBER_OF_PATTERN_ATTRIBUTES;
    start_angle = PATTERN[base_ind + PATTERN_START_ANGLE_OFFSET];    
    end_angle = PATTERN[base_ind + PATTERN_END_ANGLE_OFFSET];    
    flowRate = PATTERN[base_ind + PATTERN_FLOW_OFFSET];
    if (!protocol){Serial.println(flowRate);}
    flowRate = ALGORITHM.update(flowRate, sweep_count); // Get the next flow rate for the set flow rate
    if (!protocol){Serial.println(flowRate); Serial.println(sweep_count);}


    //********UPDATE SPEED***************
    int rot_speed = ALGORITHM.speed(flowRate);

  // Reverses the direction of the spray when going the other way
    if (cur_dir < 0){
      int temp = start_angle;
      start_angle = end_angle;
      end_angle = temp;
    }
    // Other than the first rotation this sould be zero steps most of the time.
    rotate(start_angle, MAX_MOTOR_SPEED);
    flow(flowRate);
    rotate(end_angle, rot_speed);
    
    index += cur_dir;
    // Hit end of pattern, swap direction
    if (index == pattern_index)
    { index = index - 1; //So it repeats on the end.
      cur_dir = -1;
      sweep_count++;
      if(runOnce){
        break;
      }
    }
    // Hit end of pattern, swap direction
    else if (index < 0)
    { index = 0;  //So it repeats on the end.
      cur_dir = 1;
      sweep_count++;
    }
  }
  shutflow();
  home();
}

/* Prints out the flow rates that will be sprayed for the given flow rate. */
void getFlowRates(int topFlow)
{
  Serial.print("Flow Rates in pattern when FR = "); Serial.println(topFlow);
  for(int sweep_count = 0; sweep_count < NUMBER_OF_SWEEPS; sweep_count++)
  {
    int flowRate = ALGORITHM.update(topFlow, sweep_count);
    Serial.print(flowRate);    Serial.print(" ");
  }
  Serial.println();
}

/* Sets the debug value to be true for all the motors. Prints out more information when doing tasks. */
void debug(){
  for (int i = 0; i < NUMBER_OF_ZONES; i++)
  {
    SPRINKLER[i].setdebug(!SPRINKLER[i]._debug); /*** CURRENTLY WEIRD ERROR WHERE AFTER THIS FUNCTION IT IS NO LONGER THE OPPOSITE ***/
  }
  flowStepper.setdebug(!flowStepper._debug);
}

void raspberry_pi(){
  rasp_pi = !rasp_pi;
}

/* The loop that the arduino contiously runs. Deals with reading in the commands and handeling them */
void loop()
{
  if (Serial.available() > 0) // check for serial commands
  {
    char command[8][MAX_COMMAND_LENGTH];     // An array of commands that will be parsed. 
    memset(command, 0, sizeof(command));     // Completely wipe the memory of the string.
    char str[MAX_COMMAND_LENGTH];           //Define the string the input will be pulled to.
    memset(str, 0, sizeof(str));                         // Completely wipe the memory of the string once again, just in case.
    Serial.readBytesUntil(CR, str, MAX_COMMAND_LENGTH);  // Pull the serial input to the string, stopping at CR.
    int num_arg = parseProtocol(command, str);        // Number of arguments

    /* Parse the protocol commands first so it doesn't print anything to the serial port unless needed.*/
    protocol = true;  // Assume it is a protocol command untill we have checked.
    // get Current settings
    if (strcmp(command[0], "cs") == 0){
      getCurrentSettings(); 
    }
    // Add to pattern
    else if (strcmp(command[0], "pro_addpat") == 0){
      //  Start angle,     End angle,         direction,        flowRate,        speed              repeat
      addToSprayPattern(atoi(command[1]), atoi(command[2]), atoi(command[3]), atoi(command[4]), atof(command[5]), atoi(command[6]));
    }
    // Change zone
    else if (strcmp(command[0], "cz") == 0){
      setZone(atoi(command[1]));
    }
    else if (strcmp(command[0], "pro_run") == 0){
      runStoredSprayPattern();
    }
    else if (strcmp(command[0], "pro_shutdown") == 0){
      shutDown();
    }
    // Testing commands
    else if (strcmp(command[0], "ping") == 0){
      ping();
    }
    else
    {
      protocol = false;
    }

    // Protocol command not found, look for other commands
    if (!protocol){
      Serial.print("Calling Command ");
      Serial.print(command[0]);
      Serial.print(" with arguments: ");
      for(int i = 1; i < num_arg - 1; i++) { 
        Serial.print(command[i]); 
        Serial.print(" ");
      }
      Serial.println();
      if (!running) {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // KEYBOARD
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        //rotation / sprinkler
        if (num_arg == 4 && strcmp(command[0], "rt") == 0) rotate(atoi(command[1]), atoi(command[2]));
        else if (strcmp(command[0], "rt") == 0) rotate(atoi(command[1]));
        else if (strcmp(command[0], "D") == 0) rotate(sprinklerStepper.getPosition() + KEYLARGE);
        else if (strcmp(command[0], "d") == 0) rotate(sprinklerStepper.getPosition() + KEYSMALL);
        else if (strcmp(command[0], "A") == 0) rotate((sprinklerStepper.getPosition() - KEYLARGE));
        else if (strcmp(command[0], "a") == 0) rotate((sprinklerStepper.getPosition() - KEYSMALL));    
        else if (strcmp(command[0], "h") == 0) home();
        else if (strcmp(command[0], "b") == 0) Serial.println(sprinklerStepper.getDirection());     
        else if (strcmp(command[0], "sd") == 0) Serial.println(getAngle());
        //flow
        else if (num_arg == 4 && strcmp(command[0], "of") == 0) flow(atoi(command[1]), atoi(command[2]));
        else if (strcmp(command[0], "of") == 0) flow(atoi(command[1]));
        else if (strcmp(command[0], "W") == 0) flow(flowStepper.getPosition() + KEYLARGE);
        else if (strcmp(command[0], "w") == 0) flow(flowStepper.getPosition() + KEYSMALL);
        else if (strcmp(command[0], "S") == 0) flow((flowStepper.getPosition() - KEYLARGE));
        else if (strcmp(command[0], "s") == 0) flow((flowStepper.getPosition() - KEYSMALL));    
        else if (strcmp(command[0], "sf") == 0) shutflow();
        else if (strcmp(command[0], "v") == 0) Serial.println(flowStepper.getDirection());
        else if (strcmp(command[0], "ss") == 0) Serial.println(getFlow()); 

        //control
                                                            //Start angle      // End angle      // Flow rate
        else if (strcmp(command[0], "ca") == 0) changeArray(atoi(command[1]), atoi(command[2]), atoi(command[3]));
        else if (strcmp(command[0], "zero") == 0) zeroCurrentZone();
        else if (strcmp(command[0], "test") == 0) runZone(true);
        else if (strcmp(command[0], "n") == 0) saveAndMove();
        else if (strcmp(command[0], "print") == 0) printArray();
        else if (strcmp(command[0], "gfr") == 0) getFlowRates(atoi(command[1]));        
        else if (strcmp(command[0], "fsleep") == 0) flowStepper.sleep(SLEEP); 
        else if (strcmp(command[0], "fwake") == 0) flowStepper.sleep(AWAKE); 
        else if (strcmp(command[0], "ssleep") == 0) sprinklerStepper.sleep(SLEEP);
        else if (strcmp(command[0], "swake") == 0) sprinklerStepper.sleep(AWAKE);
        else if (strcmp(command[0], "reset") == 0) softReset(); 
        else if (strcmp(command[0], "debug") == 0) debug();
        else if (strcmp(command[0], "raspberry") == 0) raspberry_pi();
        //shutdown
        else if (strcmp(command[0], "c") == 0) {
          Serial.println("Full Shutdown Initiated.....");
          shutflow();
          home(); 
          Serial.println(".....Complete");
        }
        else if (strcmp(command[0], "p") == 0) {      
          saveAndMove();
        }
        // Test / Setup commands
        else if (strcmp(command[0], "run") == 0) {      
          runZone(); // Run the current zone once
        }
        
        // Pattern Commands
//        else if (strcmp(command[0], "clrpat") == 0){ // Clears pattern
//          clearPattern();
//        }
//        else if (strcmp(command[0], "runpat") == 0){
//          runStoredPattern();
//        }
        else if (strcmp(command[0], "ppat") == 0){
          printPattern();
        }
//        else if (strcmp(command[0], "addpat") == 0){
//          //  Start angle,     End angle,         direction,        flowRate,        speed              repeat
//          storeSprayPattern(atoi(command[1]), atoi(command[2]), atoi(command[3]), atoi(command[4]), atof(command[5]), atoi(command[6]));
//        }
        //error
        else {
          Serial.print("Command Not Found, Try Again.");
        }

      }//if !running
    }


    // Clean up the command array.
    memset(str, 0, sizeof(str));                         // Completely wipe the memory of the string
    for (int i = 0; i < MAX_COMMAND_LENGTH; i++) {
      for (int j = 0; j < 8; j++) {
        command[j][i] = NULL;            
      }
    }
    Serial.println(); 
    if (rasp_pi && protocol){
      Serial.println("Command Done");
    }
  }//if serial available

  // Check if zone pin is set
  for (int zone = 0; zone < NUMBER_OF_ZONES; zone++)
  {
    if (digitalRead(ZONE_INPUT_PORTS[zone]) > 0)
    {
      int old_zone = CURRENT_ZONE;
      setZone(zone);
      runZone();
      setZone(old_zone);
    }
  }
} // end of loop()

/*************************************
 *  Protocol Functions for communication between Raspberry Pi and Arduino
 ***************************************/
/* Function to pre-process commands looking for protocol messages instead.
Puts the command into the command string array as well as returns the number of arguments found.
*/
int parseProtocol(char command[8][40], char str[MAX_COMMAND_LENGTH]){
  /* This function defines a command line interface using the format
   *   command arg arg ...
   * If longer commands are needed, change both MAX_COMMAND_LENGTH and the second array length in the function declaration.
   */

  char * pch;                              // Define the string each component will be pulled to.
  pch = strtok(str, " ");                  // Take the first part of str, up to a space.  
  int arg = 0;
  for (int i = 0; pch[i] != NULL; i++)
  {
    command[arg][i] = pch[i];             // Get the name of the command itself.
  }
  arg++;
  while(pch != NULL) {                    // While there are still arguments, populate the command array.
    pch = strtok(NULL, " ");
    for (int i = 0; pch[i] != NULL; i++)
    {
      command[arg][i] = pch[i];            // Populate the list of commands with components.
    }
    arg++;
  }
  return arg;
}


/* Returns the current flow followed by the current degree. */
void getCurrentSettings(){
  Serial.print(flowStepper.getPosition()); 
  Serial.print(" ");
  Serial.println(sprinklerStepper.getPosition()); 
}

/* Adds to the spray pattern, only responds back with ACK. 
*** NOT CURRENTLY USED. WAS USED BY THE RASP PI. SHOULD BE MERGED WITH FUNCTION ABOVE USING PROTOCOL VARIABLE*/
void addToSprayPattern(int start_angle, int end_angle, int direc, int flowRate, float rot_speed, int repeat){
  // Store the values in the pattern array
  if (pattern_index >= MAX_NUMBER_OF_PATTERN_STEPS)
  {
    return; 
  }
  int start_index = pattern_index * NUMBER_OF_PATTERN_ATTRIBUTES;
  PATTERN[start_index + PATTERN_START_ANGLE_OFFSET] = start_angle;
  PATTERN[start_index + PATTERN_END_ANGLE_OFFSET] = end_angle;
  //PATTERN[start_index + PATTERN_DIRECTION_OFFSET] = direc;
  //PATTERN[start_index + PATTERN_REPEAT_OFFSET] = repeat;
  PATTERN[start_index + PATTERN_FLOW_OFFSET] = flowRate;
  //PATTERN_SPEED_ARRAY[pattern_index] = rot_speed * DEFAULT_SPRINKLER_ROTATE_SPEED;

  pattern_index = pattern_index + 1;  
  Serial.print("ACK");
}

/* Runs the stored pattern, responding with an ACK when finished.
*** NOT CURRENTLY USED. WAS USED BY THE RASP PI. SHOULD BE MERGED WITH FUNCTION ABOVE USING PROTOCOL VARIABLE*/
void runStoredSprayPattern(){
  int start_angle, end_angle, dir, flowRate, repeat, base_ind;
  float rot_speed;
  for (int index = 0; index < pattern_index; index++){
    base_ind = index * NUMBER_OF_PATTERN_ATTRIBUTES;
    start_angle = PATTERN[base_ind + PATTERN_START_ANGLE_OFFSET];    
    end_angle = PATTERN[base_ind + PATTERN_END_ANGLE_OFFSET];    
    flowRate = PATTERN[base_ind + PATTERN_FLOW_OFFSET];
    //rot_speed = PATTERN_SPEED_ARRAY[index];
    // Set speed
    if (rot_speed > MIN_MOTOR_SPEED)  // Note: Min Motor speed is a high number
    { 
      rot_speed = MIN_MOTOR_SPEED;
    }
    else if (rot_speed < MAX_MOTOR_SPEED) // Note: max motor speed is a low number
    {
      rot_speed = MAX_MOTOR_SPEED; 
    }

    rotate(start_angle, MAX_MOTOR_SPEED);
    flow(flowRate);
    rotate(end_angle, rot_speed);
  } // End For
  Serial.print("ACK");
}

/* Sends both flow and current head home. */
void shutDown(){
  // Shut flow
  flowStepper.sleep(AWAKE);
  flowStepper.home();
  flowStepper.sleep(SLEEP);
  // Rotate to home for sprinkler head
  sprinklerStepper.sleep(AWAKE);
  sprinklerStepper.home();
  sprinklerStepper.sleep(SLEEP);
  Serial.print("ACK");
}

/* Set the new zone to passed in value */
void setZone(int new_zone){ 
  if ((new_zone >= 0) && (new_zone < NUMBER_OF_ZONES)){
    CURRENT_ZONE = new_zone; // Set new zone
    sprinklerStepper = SPRINKLER[CURRENT_ZONE]; // Set new sprinkler head
    pattern_index = 0; // Remove current pattern
    Serial.print("ACK");
  }
}

/* Util Commands  */
/* Used to see if arduino is responsive. Responds with the current uptime. */
void ping(){
  unsigned long time = millis();
  Serial.print(time);
}
/* Resets the arduino. Soft reset as there may be pins that do not get reset as well. */
void softReset(){
  asm volatile (" jmp 0");
}


