#include <AccelStepper.h>             //Stepper Motor library - can be downloaded from http://www.airspayce.com/mikem/arduino/AccelStepper/
#include <Servo.h>
#include <MIDI.h>                     //MIDI library - can be downloaded from https://github.com/FortySevenEffects/arduino_midi_library/releases/tag/4.2

// ****************************************
// TODO List:
// ****************************************
/*
 * - Pick Angle adjustment
 * - Control Change options (see below)
 * - Varying fretting strength as angle changes
 * - NOTE - Angle adjustment needs limit switch of some kind
 * - Remove clamping (emergency reset)
 */
// ****************************************
// 
// ****************************************

//------------ Global Variables ------------
const bool DEBUG = true;                          // sends debug info via Serial if true
bool dampEveryNote = false;                        // sets whether each note is damped on Note Off (default TRUE)
bool continuousPlaying = false;                   // sets whether the monochord keeps picking between Note ON and Note OFF messages (Continuous), or picks only once (default FALSE)

int pickSwitchPin = 22;

int sol1 = 49;
int sol2 = 46;
bool damperOn = false;                            // flag to tell if damper is on or not

// Fretter Servos
Servo pitchServo;                                 // servo that rotates the fretting rods into correct position along the string
Servo fretterServo;                               // servo that raises or lowers the fretting rod onto the string

const int PitchServoPin = 48;
//const int pitchMaxValue = 120;
//const int pitchMinValue = 63;                     // absolute minimum value (run to 70 degrees to ensure no collision on overshoot)

// Measurements for Scale (String) Length: 805 mm - Measurements in mm from Nut
//int fretREALPositions[12] = {45.14, 87.74, 127.96, 165.92, 201.75, 235.58, 267.5, 297.64, 326.09, 352.94, 378.29, 402.21};
// Perpendicular Distance between point of rotation and string = ~180mm
const int fretPerpAngle = 95;                     // angle at which the fretter is perpendicular to the string
// array holds angle values for each fretting position of the 12 notes in an octave
int fretOffset = 0;
int fretPositions[12] = {69, 76, 83, 91, 100, 110, 119, 127, 134, 140, 146, 149};
int fretClampings[12] = {125, 125, 130, 130, 135, 135, 135, 135, 130, 130, 125, 125};
// E Tuning - F3 F* G G* A A* B C4 C* D D* E4
// MIDI - 0x35 0x36 0x37 0x38 0x39 0x3A 0x3B 0x3C 0x3D 0x3E 0x3F 0x40 --> Need adjustment value of -65 to get correct Fret Index
const int midiMin = 0x35;
const int midiMax = 0x40;
const int midiOffset = -midiMin;

const int FretterServoPin = 47;
const int fretterOn = 130;
const int fretterTouching = 115;
const int fretterClose = 112;
const int fretterOff = 105;

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

const int pickValue = -53;                        // 200 steps per revolution -> so this should be a quarter of a revolution
const int maxAngleSteps = 2000;                    // NOTE - Placeholder value, needs testing

AccelStepper pickStepper(AccelStepper::DRIVER, StpSTP, StpDIR);     //(mode, STEP pin, DIR pin)
AccelStepper angleStepper(AccelStepper::DRIVER, angleStpSTP, angleStpDIR); 

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midi1)

//------------ Global Setup ------------
void setup() {
  // MIDI - setup
  midi1.setHandleNoteOn(handleNoteOn);
  midi1.setHandleNoteOff(handleNoteOff);
  midi1.setHandleControlChange(handleControlChange);
  midi1.begin(MIDI_CHANNEL_OMNI);                                     // listen on all channels
  
  // Fretter Servos - setup
  pitchServo.attach(PitchServoPin);
  fretterServo.attach(FretterServoPin);
  pitchServo.write(fretPerpAngle);
  fretterServo.write(fretterOff);
  
  // Solenoid Damper - setup
  pinMode(sol1, OUTPUT);     
  pinMode(sol2, OUTPUT);   

  // Stepper Motor - setup
  pickStepper.enableOutputs();
  pickStepper.setCurrentPosition(0);
  pickStepper.setMaxSpeed(1000);
  pickStepper.setSpeed(1000);  
  pickStepper.setAcceleration(20000);
  //pickStepper.disableOutputs();

  angleStepper.enableOutputs();
  angleStepper.setCurrentPosition(0);
  angleStepper.setMaxSpeed(1000);
  angleStepper.setSpeed(1000);  
  angleStepper.setAcceleration(20000);
  
  pinMode(21, INPUT);
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
  /* Things to Implement:
   * - ?Pick Angle
   * - Set continuous playing / non-continuous (default)
   */
}

void pickString() {
  //angleStepper.runToNewPosition(angleStepper.currentPosition() + pickValue);    // BLOCKING
  pickStepper.runToNewPosition(pickStepper.currentPosition() + pickValue);    // BLOCKING
  //pickStepper.moveTo(pickStepper.currentPosition() + pickValue);
}

void dampString() {
  dampOn();
  //delay(100);
  //dampOff();                                                                    // turn off when note has decayed
}

void dampOn() {
  damperOn = true;
  digitalWrite(sol1, HIGH);
  digitalWrite(sol2, HIGH);
  delay(50);
}

void dampOff() {
  if (damperOn) {
    damperOn = false;
    digitalWrite(sol1, LOW);
    digitalWrite(sol2, LOW);
    delay(50);
  }
}

void moveFretter(byte pitch) {
  //fretterServo.write(fretterOff);
  int fretNum = pitch + midiOffset;
  pitchServo.write(fretPositions[fretNum] + fretOffset);
  // NOTE - need to slow movement to extreme angles to prevent overshoot
  // NOTE - need to change damping strength as angle changes
  delay(10);
//  fretterServo.write(fretterOn);
  fretOn();
}

void fretOn() {
  dampOn();
  fretterServo.write(fretterClose);
  delay(10);
  fretterServo.write(fretterTouching);
  delay(10);
  fretterServo.write(fretterOn);
  delay(10);
  dampOff();
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
  continuousPlaying = !continuousPlaying;
}

// Changes the picking angle between 0 and 45 degrees, scales between received value of 0 - 127
void setPickAngle(byte value) {
  if (DEBUG) Serial.println("Changing Picking Angle");
  int newAnglePosition = maxAngleSteps / 127 * value;
  angleStepper.runToNewPosition(newAnglePosition);
  if (DEBUG) Serial.println(newAnglePosition);
}

//------------ Main Loop ------------
void loop() {
  //midi1.read();
  //pickStepper.run();
  Serial.println(digitalRead(22));
  
//  pickStepper.runToNewPosition(pickStepper.currentPosition() + pickValue);
//  delay(1000);

//  handleNoteOn(1,5,50);
//  delay(1000);
//  handleNoteOff(1,5,50);
//  delay(2000);

//  angleStepper.runToNewPosition(angleStepper.currentPosition() + pickValue);
//  delay(1000);
//  Serial.println(angleStepper.currentPosition());
}
