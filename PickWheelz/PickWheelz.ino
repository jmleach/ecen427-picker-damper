#include <AccelStepper.h>             //Stepper Motor library - can be downloaded from http://www.airspayce.com/mikem/arduino/AccelStepper/
#include <Servo.h>
#include <MIDI.h>                     //MIDI library - can be downloaded from https://github.com/FortySevenEffects/arduino_midi_library/releases/tag/4.2

// ****************************************
// TODO List:
// ****************************************
/*
 * - Find damper ON and OFF values
 * - Find steps between picks
 * - Implement Pick Stepper Homing
 * - Control Change functions
 */
// ****************************************
// 
// ****************************************

//------------ Global Variables ------------
const bool DEBUG = true;              // sends debug info via Serial if true

Servo damperServo;
const int DamperPin = 48;                       // NOTE servo1 pin??? -> need to confirm
const int defaultDampOn = 180;                  // used to reset after damper strength control change
const int defaultDampOff = 0;                   // used to reset after damper strength control change
const int dampOnValue = defaultDampOn;          // NOTE placeholder value -> needs testing
const int dampOffValue = defaultDampOff;        // NOTE placeholder value -> needs testing

// Stepper Motor Pins
const int StpRefP = A15;
const int StpMS1P = 36; 
const int StpMS2P = 34; 
const int StpMS3P = 32; 
const int StpRSTP = 30;
const int StpSLPP = 28;
const int StpSTPP = 26;
const int StpDIRP = 24;

const int angleStpSTPP = 25;
const int angleStpDIRP = 23;

AccelStepper pickStepper(AccelStepper::DRIVER, StpSTPP, StpDIRP);     //(mode, STEP pin, DIR pin)
//AccelStepper angleStepper(AccelStepper::DRIVER, angleStpSTPP, angleStpDIRP); 
const int pickValue = 50;                       // NOTE number of steps between one pick and the next pick -> needs testing

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midi1)

//------------ Global Setup ------------
void setup() {
  // MIDI - setup
  midi1.setHandleNoteOn(handleNoteOn);
  midi1.setHandleNoteOff(handleNoteOff);
  midi1.setHandleControlChange(handleControlChange);
  midi1.begin(MIDI_CHANNEL_OMNI);                                     // listen on all channels

  // RC Servo - setup
  damperServo.attach(DamperPin);
  damperServo.write(dampOffValue);

  // Stepper Motor - setup
  pickStepper.setCurrentPosition(0);
  pickStepper.setMaxSpeed(1000);
  pickStepper.setAcceleration(10000);

  if (DEBUG) Serial.begin(9600);
  if (DEBUG) Serial.println("Setup Complete");
}

//------------ MIDI Functions ------------
void handleNoteOn(byte channel, byte pitch, byte velocity){
  if (DEBUG) Serial.println("Note On");
  
  if (velocity > 0){
    damperServo.write(dampOffValue);                                  // turn off damper
    pickStepper.moveTo(pickStepper.currentPosition() + pickValue);    // pick string *****TEMP SOLUTION???*****
  }
  else {                                                              // velocity = 0 counts as NOTE OFF
    damperServo.write(dampOnValue);
  }
}

void handleNoteOff(byte channel, byte pitch, byte velocity){
  if (DEBUG) Serial.println("Note Off");

  damperServo.write(dampOnValue);                                     // turn on damper
}

void handleControlChange(byte channel, byte pitch, byte velocity){
  if (DEBUG) Serial.println("CC");
  
  // TODO
  
  /* Things to Implement:
   * - Damper Strength
   * - Pick Angle
   */
}

//------------ Main Loop ------------
void loop() {
  //midi1.read();
  handleNoteOff(0,5,50);
  delay(1000);
  handleNoteOn(0,5,50);
  delay(1000);
}
