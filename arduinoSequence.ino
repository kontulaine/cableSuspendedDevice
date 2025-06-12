#include "arduino_secrets.h"

/*
UPLOAD THIS ONLY WHEN USING WITH PYTHON TESTING!
*/
#include <AccelStepper.h>
#include <Math.h>

const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];    // temporary array for use when parsing

// variables to hold the parsed data
char userCommand[numChars] = {0};
int xCoordinate = 0;
int yCoordinate = 0;

boolean newData = false;

// Init steppers and pins
const int enablePin = 8;
AccelStepper stepper1(1, 2, 5);  // (1, stepPinX, dirPinX) // LEFT STEPPER
AccelStepper stepper2(1, 4, 7);  // (1, stepPinZ, dirPinZ) // RIGHT STEPPER

// Amount of steps per 1 mm rotation of gear wheel 
const float STEPS_PER_MM = 1.6;

// Distance between stepper motors in mm
const long d = 910;

// Stepper motor positions in mm, (x1, y1) left stepper, (x2, y2) right stepper
const long x1 = -d/2;
const long y1 = d/2;
const long x2 = x1 + d;
const long y2 = d/2;

const float initialLength1 = round(sqrt(x1*x1 + y1*y1));
const float initialLength2 = round(sqrt(x2*x2 + y2*y2));

int movCount = 0; // variable to track amount of movements

long prevLength1, prevLength2; // The previous lengths which are updated after each position change

void calculateSteps(long x, long y, long &steps1, long &steps2, long &prevLength1, long &prevLength2) {
  // This function calculates the steps required to move the 
  // object to the desired position and calls moveToPosition

  // lengths of the cables from the flywheels to the current position
  float L1 = round(sqrt((x-x1)*(x-x1) + (y-y1)*(y-y1)));
  float L2 = round(sqrt((x-x2)*(x-x2) + (y-y2)*(y-y2)));

  //Serial.println("Lenghts:");
  //Serial.println(L1);
  //Serial.println(L2);

  // convert lengths to steps
  steps1 = round((L1 - prevLength1) * STEPS_PER_MM);
  steps2 = round((prevLength2 - L2) * STEPS_PER_MM);

  // update previous lengths to the new ones
  prevLength1 = L1;
  prevLength2 = L2;

}

void moveToPositionSynced(long steps1, long steps2){
  // Move the motors to the calculated steps in a synchronised manner
  // by scaling their speeds and accelerations so that they end up at the destination at the same time


  movCount++; // Increment counter by 1

  if (steps1 != 0 && steps2 != 0){
    //Serial.println("Moving motors to positions (steps):");
    //Serial.println("Motor 1: ");
    //Serial.println(steps1);
    //Serial.println("Motor 2: ");
    //Serial.println(steps2);
  }

  long abs1 = abs(steps1);
  long abs2 = abs(steps2);
  long maxSteps = max(abs1, abs2);
  
  //if (maxSteps == 0){
    //Serial.println("Error: Division by zero. Steps were calculated incorrectly");
    //return;
  //}

  float ratio1 = (float)abs1 / maxSteps;
  float ratio2 = (float)abs2 / maxSteps;

  // Set minimum and maximum values for speed and acceleration
  float minSpeed = 500;
  float maxSpeed = 1000;
  float minAccel = 500;
  float maxAccel = 1200;

  // The motors arrive at their destinations at the same time
  stepper1.setMaxSpeed(minSpeed + (maxSpeed - minSpeed) * ratio1);
  stepper2.setMaxSpeed(minSpeed + (maxSpeed - minSpeed) * ratio1);
  stepper1.setAcceleration(minAccel + (maxAccel - minAccel) * ratio1);
  stepper2.setAcceleration(minAccel + (maxAccel - minAccel) * ratio2);

  stepper1.move(steps1);
  stepper2.move(steps2);

  while((stepper1.isRunning()) || (stepper2.isRunning())){
    // run the motors
    stepper1.run();
    stepper2.run();
  }
  Serial.println("DONE");
}


int sgn(long value) {
  // Helper function to get the sign of a value
  return (value > 0) - (value < 0);
}

// data parsing from this tutorial:
// https://forum.arduino.cc/t/serial-input-basics-updated/382007/3
// Example 5 - Receive with start- and end-markers combined with parsing


void recvWithStartEndMarkers() {
  /*This function sets the end and start markers of the message
  and saves the relevant information into the receivedChars array
  */
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
    while (Serial.available() > 0 && newData == false) {
        //Serial.println("Detected data");
        rc = Serial.read();
        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void parseData() {      
  // split the data into its parts
  char * strtokIndx; // this is used by strtok() as an index

  strtokIndx = strtok(tempChars,",");   // get the first part - the string command
  strcpy(userCommand, strtokIndx);  // copy it to userCommand

  strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
  xCoordinate = atoi(strtokIndx);     // convert x coordinate to an integer

  strtokIndx = strtok(NULL, ",");
  yCoordinate = atoi(strtokIndx);     // convert y coordinate to an integert
}

void setup() {

  // serial port open
  Serial.begin(9600);

  // Welcome message
  //Serial.println("Welcome!");
  //Serial.println(initialLength1);
  //Serial.println(initialLength2);
  /*
  Serial.println("The current position is:");
  Serial.println("Motor 1:");
  Serial.println(stepper1.currentPosition());
  Serial.println("Motor 2:");
  Serial.println(stepper2.currentPosition());
  Serial.println("To move the robot to the desired position, give your commands in this style: <MOVE, X, Y>");
  */
  
  // enable the driver
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW);  

  // set max speed and accel of the motor

  stepper1.setMaxSpeed(1000);
  stepper1.setAcceleration(1200);
  stepper2.setMaxSpeed(1000);
  stepper2.setAcceleration(1200);

  Serial.println("setup done");
}

void loop() {
    //Serial.println("starting loop");
    recvWithStartEndMarkers();
    if (newData == true){
       // this temporary copy is necessary to protect the original data
      // because strtok() used in parseData() replaces the commas with \0
      strcpy(tempChars, receivedChars);
      parseData();
      newData = false;
      if( (abs(xCoordinate) <= abs(x1)) && (abs(yCoordinate) <= abs(y1)) ){  // check that new coordinates are within boundaries
        long steps1, steps2;
        if (movCount == 0){
          prevLength1 = initialLength1;
          prevLength2 = initialLength2;
        }
        calculateSteps(xCoordinate, yCoordinate, steps1, steps2, prevLength1, prevLength2);
        moveToPositionSynced(steps1, steps2);
      }
      else {
        //Serial.println("Given coordinates have to be within bounds");
      }
    }
}



