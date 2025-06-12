#include "arduino_secrets.h"

/*
Cable-suspended probe movement device
Read bachelor thesis for more information
*/

#include <AccelStepper.h>
#include <Math.h>
#include <Timer.h>

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
AccelStepper stepper1(1, 2, 5);  // (1, stepPinX, dirPinX) -- LEFT STEPPER
AccelStepper stepper2(1, 4, 7);  // (1, stepPinZ, dirPinZ) -- RIGHT STEPPER

// Amount of steps per 1 mm rotation of pulley -- NEEDS TO BE CHANGED ACCORDING TO PULLEYS USED ( k = s_tot / 2*pi*r_pulley )
const float STEPS_PER_MM = 1.6;

// Distance between stepper motors in mm
long MOTOR_DISTANCE;

// Stepper motor positions in mm, (x1, y1) left stepper, (x2, y2) right stepper
long x1;
long y1;
long x2;
long y2;

float initialLength1;
float initialLength2;

int movCount = 0; // variable to track amount of movements

long prevLength1, prevLength2; // The previous lengths which are updated after each position change
long prevX, prevY;  // previous x and y coordinates

void setMotorPositions(){
  // Function for updating stepper motor positions and initial lenghts (in mm)
  Serial.println("Updating motor positions");
  x1 = -MOTOR_DISTANCE/2;
  y1 = MOTOR_DISTANCE/2;
  x2 = x1 + MOTOR_DISTANCE;
  y2 = y1;
  initialLength1 = round(sqrt(x1*x1 + y1*y1));
  initialLength2 = round(sqrt(x2*x2 + y2*y2));
}

void calculateSteps(long x, long y, long &steps1, long &steps2, long &prevLength1, long &prevLength2) {
  // This function calculates the steps required to move the 
  // object to the desired position and calls moveToPosition

  // lengths of the cables from the motors to the current position
  float L1 = round(sqrt((x-x1)*(x-x1) + (y-y1)*(y-y1)));
  float L2 = round(sqrt((x-x2)*(x-x2) + (y-y2)*(y-y2)));

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
    Serial.println("Turning motors by (steps):");
    Serial.print("Motor 1: ");
    Serial.println(steps1);
    Serial.print("Motor 2: ");
    Serial.println(steps2);
  }

  Timer timer(MILLIS);
  timer.start();
  long abs1 = abs(steps1);
  long abs2 = abs(steps2);
  long maxSteps = max(abs1, abs2);
  
  if (maxSteps == 0){
    Serial.println("Error: Division by zero. Steps were calculated incorrectly");
    return;
  }

  // The speeds and accelerations of the motors are scaled according to the ratio
  float ratio1 = (float)abs1 / maxSteps;
  float ratio2 = (float)abs2 / maxSteps;

  // Set minimum and maximum values for speed and acceleration
  float minSpeed = 500;
  float maxSpeed = 1000;
  float minAccel = 500;
  float maxAccel = 1200;

  // Scale the speed and accelerations so that the
  // motors arrive at their destinations at the same time
  stepper1.setMaxSpeed(minSpeed + (maxSpeed - minSpeed) * ratio1);
  stepper2.setMaxSpeed(minSpeed + (maxSpeed - minSpeed) * ratio2);
  stepper1.setAcceleration(minAccel + (maxAccel - minAccel) * ratio1);
  stepper2.setAcceleration(minAccel + (maxAccel - minAccel) * ratio2);

  stepper1.move(steps1);
  stepper2.move(steps2);

  while((stepper1.isRunning()) || (stepper2.isRunning())){
    // run the motors
    stepper1.run();
    stepper2.run();
  }

  timer.stop();
  Serial.println("DONE");
  Serial.print("Time elapsed in ms:");
  Serial.println(timer.read());
}


int sgn(long value) {
  // Helper function to get the sign of a value
  return (value > 0) - (value < 0);
}

void receiveCommand() {
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


void parseCommand() {      
  // This function parses the different commands and saves appropriate values to global variables
  // For example MOVE command saves coordinates to global xCoordinate and yCoordinate
  // Commands are generally of form <CMD, X, Y>. 

  char * strtokIndx;

  strtokIndx = strtok(tempChars,",");   // get the first part - the string command
  strcpy(userCommand, strtokIndx);  // copy it to userCommand

  if(strcmp(userCommand, "MOVE") == 0){
    // save the coordinates
    strtokIndx = strtok(NULL, ","); // this continues where the previous call left off
    xCoordinate = atoi(strtokIndx);     // convert x coordinate to an integer
    strtokIndx = strtok(NULL, ",");
    yCoordinate = atoi(strtokIndx);     // convert y coordinate to an integer
  }
  else if (strcmp(userCommand, "LEN") == 0){
    strtokIndx = strtok(NULL, ",");
    MOTOR_DISTANCE = atoi(strtokIndx);
    setMotorPositions();
  }
}


void setup() {

  // serial port open
  Serial.begin(9600);

  // enable the drivers
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW);  

  // set max speed and acceleration of the motors
  stepper1.setMaxSpeed(1000);
  stepper1.setAcceleration(1200);
  stepper2.setMaxSpeed(1000);
  stepper2.setAcceleration(1200);

  // User setup and calibration
  Serial.println("Welcome!");
  Serial.println("To start, please measure the distance between the motors and give it in this style: <LEN, measured length in mm>");

  while(!newData){
    receiveCommand();
  }

  strcpy(tempChars, receivedChars);
  parseCommand();

  Serial.println("To set the probe to the home position, measure the lenghts of the cables to be the following:");
  Serial.println("Left cable:");
  Serial.println(initialLength1);
  Serial.println("Right cable:");
  Serial.println(initialLength2);

  Serial.println("To move the robot to the desired position, give your commands in this style: <MOVE, X, Y> (X and Y in mm)");
  Serial.println("To update distance between motors, use command <LEN, d> (d in mm)");
}

void loop() {
    //Serial.println("starting loop");
    receiveCommand();
    if (newData == true){
       // this temporary copy is necessary to protect the original data
      // because strtok() used in parseData() replaces the commas with \0
      strcpy(tempChars, receivedChars);
      parseCommand();
      newData = false;
      if( (abs(xCoordinate) <= abs(x1)) && (abs(yCoordinate) <= abs(y1)) ){  // check that new coordinates are within boundaries
        long steps1, steps2;
        if (movCount == 0){
          prevLength1 = initialLength1;
          prevLength2 = initialLength2;
          prevX = 0;
          prevY = 0;
        }
        if((xCoordinate != prevX) || (yCoordinate != prevY)){
          Serial.println("Moving probe to new coordinates");
          calculateSteps(xCoordinate, yCoordinate, steps1, steps2, prevLength1, prevLength2);
          moveToPositionSynced(steps1, steps2);
          prevX = xCoordinate;
          prevY = yCoordinate;
        }
      }
      else {
        Serial.print("Given coordinates have to be within bounds:");
        Serial.print(x1); Serial.print("-->"); Serial.println(y1);
      }
    }
}



