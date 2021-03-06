#include <AccelStepper.h>                         //Stepper Motor library - can be downloaded from http://www.airspayce.com/mikem/arduino/AccelStepper/
#include <Servo.h>
#include <MIDI.h>                                 //MIDI library - can be downloaded from https://github.com/FortySevenEffects/arduino_midi_library/releases/tag/4.2

// ****************************************
// TODO List:
// ****************************************
/*
 * - Solenoid PWM
 * - Pick Angle adjustment
 * - CC toggling change to use value threshold instead
 * - Varying fretting strength as angle changes
 * - NOTE - Angle adjustment needs limit switch of some kind
 * - Remove clamping (emergency reset)??
 */
// ****************************************
// 
// ****************************************

//------------ Global Variables ------------
const bool DEBUG = true;                          // sends debug info via Serial if true
bool dampEveryNote = false;                       // sets whether each note is damped on Note Off (default TRUE)
bool continuousPlaying = false;                   // sets whether the monochord keeps picking between Note ON and Note OFF messages (Continuous), or picks only once (default FALSE)

const bool pickLimitSwitch = false;               // sets whether the pickwheel uses the limit switch to pick
int pickSwitchPin = 21;

int sol1 = 49;
int sol2 = 46;
bool damperOn = false;                            // flag to tell if damper is on or not

// Fretter Servos
Servo pitchServo;                                 // servo that rotates the fretting rods into correct position along the string
Servo fretterServo;                               // servo that raises or lowers the fretting rod onto the string

const int PitchServoPin = 48;
//const int pitchMaxValue = 120;
//const int pitchMinValue = 63;                                       // absolute minimum value (run to 70 degrees to ensure no collision on overshoot)

// Measurements for Scale (String) Length: 805 mm - Measurements in mm from Nut
//int fretREALPositions[12] = {45.14, 87.74, 127.96, 165.92, 201.75, 235.58, 267.5, 297.64, 326.09, 352.94, 378.29, 402.21};

// Perpendicular Distance between point of rotation and string = ~180mm
const int fretPerpAngle = 95;                                         // angle at which the fretter is perpendicular to the string

// array holds angle values for each fretting position of the 12 notes in an octave
int fretOffset = 0;
int fretPositions[12] = {59, 65, 72, 80, 86, 96, 105, 112, 118, 124, 129, 133};
//int fretClampings[12] = {125, 125, 130, 130, 135, 135, 135, 135, 130, 130, 125, 125};

// delays from 1 fret away to max 12 frets away - in milliseconds (ms)
int frettingDelay[12] = {50,60,70,80,90,100,120,140,160,180,200,220};
int currentFret = 0;                                                  // 0 - Open String

// E Tuning - F3 F* G G* A A* B C4 C* D D* E4
// MIDI - 0x35 0x36 0x37 0x38 0x39 0x3A 0x3B 0x3C 0x3D 0x3E 0x3F 0x40 --> Need adjustment value of -65 to get correct Fret Index

const int midiMin = 0x34;
const int midiMax = 0x40;
const int midiOffset = -(midiMin+1);

const int FretterServoPin = 47;
const int fretterOn = 120;
const int fretterTouching = 110;
//const int fretterClose = 112;                                       //not used anymore
const int fretterOff = 100;

// Stepper Motor Pins
const int StpRef = A15;
const int StpMS1 = 36; 
const int StpMS2 = 34; 
const int StpMS3 = 32; 
const int StpRST = 30;
const int StpSLP = 28;
const int StpSTP = 26;
const int StpDIR = 24;

const int angleStpRST = 39;
const int angleStpSLP = 41;
const int angleStpSTP = 43;
const int angleStpDIR = 45;

const int pickValue = -53;                                            // 200 steps per revolution -> so 50 should be a quarter of a revolution
const int maxAngleSteps = 2000;                                       // NOTE - Placeholder value, needs testing

AccelStepper pickStepper(AccelStepper::DRIVER, StpSTP, StpDIR);       //(mode, STEP pin, DIR pin)
//AccelStepper angleStepper(AccelStepper::DRIVER, angleStpSTP, angleStpDIR); 

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midi1)

//------------ Global Setup ------------
void setup() {
  // MIDI - Setup
  midi1.setHandleNoteOn(handleNoteOn);
  midi1.setHandleNoteOff(handleNoteOff);
  midi1.setHandleControlChange(handleControlChange);
  midi1.begin(MIDI_CHANNEL_OMNI);                                     // listen on all channels
  
  // Fretter Servos - Setup
  pitchServo.attach(PitchServoPin);
  fretterServo.attach(FretterServoPin);
  pitchServo.write(fretPerpAngle);
  fretterServo.write(fretterOff);
  
  // Solenoid Damper - setup
  pinMode(sol1, OUTPUT);     
  pinMode(sol2, OUTPUT);   

  // Stepper Motor - Setup
  pickStepper.enableOutputs();
  pickStepper.setCurrentPosition(0);
  pickStepper.setMaxSpeed(1000);
  pickStepper.setSpeed(1000);  
  pickStepper.setAcceleration(20000);
  //pickStepper.disableOutputs();

//  angleStepper.enableOutputs();
//  angleStepper.setCurrentPosition(0);
//  angleStepper.setMaxSpeed(1000);
//  angleStepper.setSpeed(1000);  
//  angleStepper.setAcceleration(20000);

  // MISC - Setup
  pinMode(pickSwitchPin, INPUT);                                      // set up pickwheel limit switch
  if (DEBUG) Serial.begin(9600);
  if (DEBUG) Serial.println("Setup Complete");
}

//------------ MIDI Functions ------------
void handleNoteOn(byte channel, byte pitch, byte velocity){
  if (DEBUG) Serial.println("Note On");
  
  if (velocity > 0){
    dampOff();                                                        // turn off damper
    if (pitch >= midiMin && pitch <= midiMax) {
      moveFretter(pitch);                                             // move fretter into position if note is within our range
    }
    pickString();                                                     // play note
  }
  else {                                                              // velocity = 0 counts as NOTE OFF
    if (dampEveryNote) dampString();
  }
  //clearInputBuffer();   //NOTE - Does not currently work
}

void handleNoteOff(byte channel, byte pitch, byte velocity){
  if (DEBUG) Serial.println("Note Off");
  
  if (dampEveryNote) dampString();                                    // turn on damper
  fretterServo.write(fretterOff);
}

void handleControlChange(byte channel, byte pitch, byte velocity){
  //if (DEBUG) Serial.println("CC: ");

  if (pitch == 0x55) toggleDamper();
  else if (pitch == 0x59) toggleDampingMode();
  else if (pitch == 0x05) togglePlayingMode();
  else if (pitch == 0x0A) setPickAngle(velocity);
}

void pickString() {
  if (pickLimitSwitch) {
    pickStepper.moveTo(pickStepper.currentPosition() - 200);                      // start a full rotation
    while(!digitalRead(pickSwitchPin)) {                                          // run pickwheel until pickLimitSwitch is activated - i.e. quarter of rotation is complete
      pickStepper.run();
    }
    pickStepper.runToNewPosition(pickStepper.currentPosition() - 10);             // make sure we move past the limit switch
  }
  else {
    pickStepper.runToNewPosition(pickStepper.currentPosition() + pickValue);      // BLOCKING
  }
}

void dampString() {
  dampOn();
  delay(100);
  dampOff();                                                                    // turn off when note has decayed
}

void dampOn() {
  damperOn = true;
  digitalWrite(sol1, HIGH);
  digitalWrite(sol2, HIGH);
  delay(5);
}

void dampOff() {
  if (damperOn) {
    damperOn = false;
    digitalWrite(sol1, LOW);
    digitalWrite(sol2, LOW);
    delay(5);
  }
}

void moveFretter(byte pitch) {
  int fretNum = pitch + midiOffset;
  if (fretNum < 0 || fretNum >= 12) return;
  int fretDiff = abs(fretNum - currentFret);
  if (fretDiff >= 6) {
    if (fretNum == 0){
      Serial.println("Slowing min value");
      pitchServo.write(fretPositions[fretNum+1] + fretOffset);
      delay(100);
    }
    else if (fretNum == 11){
      Serial.println("Slowing max value");
      pitchServo.write(fretPositions[fretNum-1] + fretOffset);
      delay(100);
    }
  }
  pitchServo.write(fretPositions[fretNum] + fretOffset);
  
  delay(frettingDelay[fretDiff]);                                                 // uses variable delay depending on the distance the fretter must travel
  fretOn();
  currentFret = fretNum;
}

void fretOn() {
  //dampOn();
  fretterServo.write(fretterTouching);
  delay(10);
  fretterServo.write(fretterOn);
  delay(50);                                                                      //NOTE - Might need reducing
  //dampOff();
  dampString();
}

// Toggle damper ON/OFF
void toggleDamper() {
  if (DEBUG) Serial.println("Damper Toggled On/Off");
  if (damperOn) dampOff();
  else dampOn();
}

// Set Damping Mode (To damp after every note or not)
void toggleDampingMode() {
  if (DEBUG) Serial.println("Damping Mode Toggled");
  dampEveryNote = !dampEveryNote;
}

// Toggle Note Playing Mode - Continuous -> (i.e. keep picking between Note ON and Note OFF) / Non-Continuous (default) -> pick once per note
void togglePlayingMode() {
  if (DEBUG) Serial.println("Continuous Playing Toggled");
  continuousPlaying = !continuousPlaying;   //NOTE - Doesn't change anything currently
}

// Changes the picking angle between 0 and 45 degrees, scales between received value of 0 - 127
void setPickAngle(byte value) {
  if (DEBUG) Serial.println("Changing Picking Angle");
  int newAnglePosition = maxAngleSteps / 127 * value;
//  angleStepper.runToNewPosition(newAnglePosition);
  if (DEBUG) Serial.println(newAnglePosition);
}

void clearInputBuffer() {
//  while(Serial.available()){  //is there anything to read?
//    char getData = Serial.read();  //if yes, read it
//    if (DEBUG) Serial.println("PURGING STORED MIDI MESSAGE");
//  }
  while(midi1.check()) {
    midi1.read();
    if (DEBUG) Serial.println("PURGING STORED MIDI MESSAGE");
  }
}

//------------ Main Loop ------------
void loop() {
  midi1.read();
}
