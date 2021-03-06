#include <Servo.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <MS5837.h>
#include <utility/imumaths.h>

// ======================================================================================= //
//                                                                      START OF CONSTANTS // 
// ======================================================================================= //

#define LOOP_TIME_DELAY_MS (200)

long m_timestamp = 0;
long m_previousTime = 0;
long m_deltaTime = 0;
long m_startTime = 0;
long m_totalTimeElapsed = 0;

const float kDepthOfPool = 7; // in feet

const float kYawP = 0.002; // 0.004 was good, 0.002 is better
const float kYawI = 0;
const float kYawD = 0.007;
const float kPitchP = 0.005; // 0.005 was acceptable
const float kPitchI = 0;
const float kPitchD = 0;
const float kRollP = 0.005;
const float kRollI = 0;
const float kRollD = 0;
const float kDepthP = 0;
const float kDepthI = 0;
const float kDepthD = 0;

const float kMaintainDepthThrottle = 0.07;

const float kYawRateP = 0;
const float kPitchRateP = 0;
const float kRollRateP = 0;

const float kTranslationP = 0;
const float kTranslationI = 0;
const float kTranslationD = 0;
const float kRightTranslationOffset = 0.8;

const float kYawThreshold = 2;
const float kPitchThreshold = 10;
const float kRollThreshold = 10;
const float kDepthThreshold = 0.5;

// ======================================================================================= //
//                                                                        END OF CONSTANTS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                         START OF HARDWARE INSTANTIATION //
// ======================================================================================= //

Servo m_horizontalLeftMotor;
Servo m_horizontalRightMotor;
Servo m_verticalFrontLeftMotor;
Servo m_verticalFrontRightMotor;
Servo m_verticalBackLeftMotor;
Servo m_verticalBackRightMotor;

Adafruit_BNO055 m_imu;
MS5837 m_barometer;
float resetYawConstant = 0;

void instantiateMotors() {
  Serial.println("MOTORS INSTANTIATING");
  
  m_horizontalLeftMotor.attach(6);
  m_horizontalRightMotor.attach(10);
  m_verticalFrontLeftMotor.attach(3);
  m_verticalFrontRightMotor.attach(9);
  m_verticalBackLeftMotor.attach(11);
  m_verticalBackRightMotor.attach(5);
  
  stopAll();
  delay(10000);
  
  Serial.println("MOTORS INSTANTIATED");
}

void instantiateIMU() {
  Serial.println("IMU INSTANTIATING");
  
  m_imu = Adafruit_BNO055(55);
  while(!m_imu.begin())
  {
    Serial.println("IMU INIT FAILED");
    delay(5000);
  }
  delay(5000);

  imu::Vector<3> euler = m_imu.getVector(Adafruit_BNO055::VECTOR_EULER);
  resetYawConstant = euler.x();
  
  Serial.println("IMU INSTANTIATED");
}

void instantiateBarometer() {
  Serial.println("BAROMETER INSTANTIATING");
  
//  Wire.begin(); // this line is unnecessary if you instantiate imu first
  while (!m_barometer.init()) {
    Serial.println("BAROMETER INIT FAILED!");
    delay(5000);
  }
  m_barometer.setModel(MS5837::MS5837_02BA);
  m_barometer.setFluidDensity(997); // kg/m^3 (freshwater, 1029 for seawater)
  delay(5000);
  
  Serial.println("BAROMETER INSTANTIATED");
}

// ======================================================================================= //
//                                                           END OF HARDWARE INSTANTIATION //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                       START OF STATE ESTIMATION METHODS //
// ======================================================================================= //

float m_measuredYaw = 0;
float m_measuredPitch = 0;
float m_measuredRoll = 0;

float m_measuredYawRate = 0;
float m_measuredPitchRate = 0;
float m_measuredRollRate = 0;

float m_measuredDepth = 0;
float m_measuredAltitude = 0;
float m_measuredPressure = 0;
float m_measuredTemperature = 0;

void updateIMU() {
  imu::Vector<3> euler = m_imu.getVector(Adafruit_BNO055::VECTOR_EULER);
  m_measuredYaw = euler.x() - resetYawConstant;
  m_measuredPitch = euler.y();
  m_measuredRoll = euler.z();

  imu::Vector<3> ang_rates = m_imu.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  m_measuredYawRate = euler.x() * RAD_TO_DEG;
  m_measuredPitchRate = euler.y() * RAD_TO_DEG;
  m_measuredRollRate = euler.z() * RAD_TO_DEG;
}

void limitOrientationMeasurements() {
  m_measuredYaw = fmod((360.0 - m_measuredYaw), 360.0);
  m_measuredRoll = fmod((360.0 - m_measuredRoll), 360.0);
  m_measuredPitch = fmod((360.0 - m_measuredPitch), 360.0);
}

void updateBarometer() {
  m_barometer.read();
  m_measuredDepth = m_barometer.depth() / 3.28084; // convert from meters to feet
  m_measuredAltitude = m_barometer.altitude() / 3.28084;
  m_measuredPressure = m_barometer.pressure();
  m_measuredTemperature = m_barometer.temperature();
}

void updateStateEstimation() {
  updateIMU();
  limitOrientationMeasurements();
  updateBarometer();
}

void displaySensorStatus() {
  Serial.println("Calibration status values: 0=uncalibrated, 3=fully calibrated");
  uint8_t system, gyro, accel, mag = 0;
  m_imu.getCalibration(&system, &gyro, &accel, &mag);
  Serial.print("CALIBRATION: Sys=");
  Serial.print(system, DEC);
  Serial.print(" Gyro=");
  Serial.print(gyro, DEC);
  Serial.print(" Accel=");
  Serial.print(accel, DEC);
  Serial.print(" Mag=");
  Serial.println(mag, DEC);
}

// ======================================================================================= //
//                                                         END OF STATE ESTIMATION METHODS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                               START OF MOVEMENT METHODS //
// ======================================================================================= //

float m_horizontalRightPower = 0;
float m_horizontalLeftPower = 0;
float m_verticalFrontRightPower = 0;
float m_verticalFrontLeftPower = 0;
float m_verticalBackRightPower = 0;
float m_verticalBackLeftPower = 0;

float m_desiredYaw = 0;
float m_desiredRoll = 0;
float m_desiredPitch = 0;
float m_desiredDepth = 0;

float m_yawControlOutput = 0;
float m_rollControlOutput = 0;
float m_pitchControlOutput = 0;
float m_depthControlOutput = 0;
float m_translationControlOutput = 0;

float m_yawRateControlOutput = 0;
float m_rollRateControlOutput = 0;
float m_pitchRateControlOutput = 0;

float m_yawError = 0;
float m_rollError = 0;
float m_pitchError = 0;
float m_depthError = 0;
float m_translationError = 0;

float m_lastYawError = 0;
float m_lastRollError = 0;
float m_lastPitchError = 0;

float directInputArray[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

/**
 * Update control loop output
 */
void calculateControlOutputs() {
  m_lastYawError = m_yawError;
  m_lastPitchError = m_pitchError;
  m_lastRollError = m_rollError;
  
  m_yawError = m_desiredYaw - m_measuredYaw;
  m_pitchError = m_desiredPitch - m_measuredPitch;
  m_rollError = m_desiredRoll - m_measuredRoll;
  m_depthError = m_desiredDepth - m_measuredDepth;

  m_yawError = (m_yawError > 180) ? m_yawError - 360 : m_yawError;
  m_yawError = (m_yawError < -180) ? m_yawError + 360 : m_yawError;
  m_pitchError = (m_pitchError > 180) ? m_pitchError - 360 : m_pitchError;
  m_pitchError = (m_pitchError < -180) ? m_pitchError + 360 : m_pitchError;
  m_rollError = (m_rollError > 180) ? m_rollError - 360 : m_rollError;
  m_rollError = (m_rollError < -180) ? m_rollError + 360 : m_rollError;

//  m_yawControlOutput = m_yawControlOutput + kYawD * ((m_yawError - m_lastYawError) / m_deltaTime);
  m_yawControlOutput = kYawP * m_yawError;
  if (m_yawControlOutput > 0.2) {
    m_yawControlOutput = 0.2;
  } else if (m_yawControlOutput < -0.2) {
    m_yawControlOutput = -0.2;
  }
//  m_yawControlOutput *= abs(m_yawControlOutput);

//  m_pitchControlOutput = kPitchP * m_pitchError - kPitchD * ((m_pitchError - m_lastPitchError) / m_deltaTime);
  m_pitchControlOutput = kPitchP * m_pitchError;
  if (m_pitchControlOutput > 0.2) {
    m_pitchControlOutput = 0.2;
  } else if (m_pitchControlOutput < -0.2) {
    m_pitchControlOutput = -0.2;
  }

//  m_rollControlOutput = kRollP * m_rollError - kRollD * ((m_rollError - m_lastRollError) / m_deltaTime);
  m_rollControlOutput = kRollP * m_rollError;
  if (m_rollControlOutput > 0.2) {
    m_rollControlOutput = 0.2;
  } else if (m_rollControlOutput < -0.2) {
    m_rollControlOutput = -0.2;
  }

  m_yawRateControlOutput = kYawRateP * -m_measuredYawRate;
  m_pitchRateControlOutput = kPitchRateP * -m_measuredPitchRate;
  m_rollRateControlOutput = kRollRateP * -m_measuredRollRate;

  m_depthControlOutput = kDepthP * m_depthError;
  m_translationControlOutput = kTranslationP * m_translationError;
}

/**
 * @param yawToCompare
 */
bool isYawAligned(float yawToCompare) {
  float error = yawToCompare - m_measuredYaw;
  error = (error > 180) ? error - 360 : error;
  error = (error < -180) ? error + 360 : error;
  return abs(error) < kYawThreshold;
}

/**
 * @param pitchToCompare
 */
bool isPitchAligned(float pitchToCompare) {
  float error = pitchToCompare - m_measuredPitch;
  error = (error > 180) ? error - 360 : error;
  error = (error < -180) ? error + 360 : error;
  return abs(error) < kPitchThreshold;
}

/**
 * @param rollToCompare
 */
bool isRollAligned(float rollToCompare) {
  float error = rollToCompare - m_measuredRoll;
  error = (error > 180) ? error - 360 : error;
  error = (error < -180) ? error + 360 : error;
  return abs(error) < kRollThreshold;
}

/**
 * @param measuredAngle
 * @param desiredAngle
 * @param threshold
 */
bool isAngleAligned(float measuredAngle, float desiredAngle, float threshold) {
  float error = desiredAngle - measuredAngle;
  error = (error > 180) ? error - 360 : error;
  error = (error < -180) ? error + 360 : error;
  return abs(error) < threshold;
}

/**
 * @param depthToCompare
 */
bool isDepthReached(float depthToCompare) {
  float error = depthToCompare - m_measuredDepth;
  return abs(error) < kDepthThreshold;
}

/**
 * @param float throttle (-1.0 to 1.0)
 * @return float input (microseconds to write to ESCs)
 */
float throttleToMicroseconds(float throttle) {
  if (throttle > 0.5) {
    throttle = 0.5;
  } else if (throttle < -0.5) {
    throttle = -0.5;
  }
  float input = throttle * 400 + 1500;
  return input;
}

/**
 * @param float input (microseconds)
 * @return float throttle (-1.0 to 1.0)
 */
float microsecondsToThrottle(float microseconds) {
  float input = (microseconds - 1500) / 500;
  return input;
}

/**
 * Direct motor control
 */
void directMotorControl() {
  m_horizontalLeftPower = directInputArray[0];
  m_horizontalRightPower = directInputArray[1];
  m_verticalFrontLeftPower = directInputArray[2];
  m_verticalFrontRightPower = directInputArray[3];
  m_verticalBackLeftPower = directInputArray[4];
  m_verticalBackRightPower = directInputArray[5];
}

float rightTranslationOffset = 1;

/**
 * Autonomous motor control (all control outputs added)
 */
void autonomousControl() {
  if (m_translationControlOutput < 0) {
    rightTranslationOffset = 1;
  } else {
    rightTranslationOffset = kRightTranslationOffset;
  }
  m_horizontalLeftPower = m_yawControlOutput + m_translationControlOutput;
  m_horizontalRightPower = -m_yawControlOutput + m_translationControlOutput * rightTranslationOffset; // left prop broke
  m_verticalFrontLeftPower = -m_rollControlOutput + m_pitchControlOutput + m_depthControlOutput + kMaintainDepthThrottle;
  m_verticalFrontRightPower = m_rollControlOutput + m_pitchControlOutput + m_depthControlOutput + kMaintainDepthThrottle;
  m_verticalBackLeftPower = -m_rollControlOutput - m_pitchControlOutput + m_depthControlOutput + kMaintainDepthThrottle;
  m_verticalBackRightPower = m_rollControlOutput - m_pitchControlOutput + m_depthControlOutput + kMaintainDepthThrottle;
}

/**
 * Stop motors
 */
void stopAll() {
  m_horizontalLeftPower = 0;
  m_horizontalRightPower = 0;
  m_verticalFrontLeftPower = 0;
  m_verticalFrontRightPower = 0;
  m_verticalBackLeftPower = 0;
  m_verticalBackRightPower = 0;
}

/**
 * Actuate motors
 */
void runMotors() {
  m_horizontalLeftMotor.writeMicroseconds(throttleToMicroseconds(-m_horizontalLeftPower));
  m_horizontalRightMotor.writeMicroseconds(throttleToMicroseconds(m_horizontalRightPower));
  m_verticalFrontLeftMotor.writeMicroseconds(throttleToMicroseconds(m_verticalFrontLeftPower));
  m_verticalFrontRightMotor.writeMicroseconds(throttleToMicroseconds(m_verticalFrontRightPower));
  m_verticalBackLeftMotor.writeMicroseconds(throttleToMicroseconds(m_verticalBackLeftPower));
  m_verticalBackRightMotor.writeMicroseconds(throttleToMicroseconds(m_verticalBackRightPower));
}

// ======================================================================================= //
//                                                                 END OF MOVEMENT METHODS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                                   START OF AUTO METHODS //
// ======================================================================================= //

bool stateRolled = false;
bool stateStabilized = false;
bool stateRotated = false;
bool stateDepthReached = false;

void stabilize() {
  m_desiredRoll = 0;
  m_desiredPitch = 0;

  stateStabilized = isRollAligned(m_desiredRoll) && isPitchAligned(m_desiredPitch);
  stateRolled = isRollAligned(m_desiredRoll);

  if (!stateRolled) {
    m_pitchControlOutput = 0;
  }
}

/**
 * Limit control outputs in effect
 */
void rotate() {
  stateRotated = isYawAligned(m_desiredYaw);

  if (!stateStabilized) {
    m_yawControlOutput = 0;
  }
}

/**
 * Go to depth
 */
void goToDepth() {
  stateDepthReached = isDepthReached(m_desiredDepth);

  if (!stateStabilized || !stateRotated) {
    m_depthControlOutput = 0;
  }
}

/**
 * Translate to based on error input (m_translationError) given rotation is aligned
 */
void translate() {
  if (!stateStabilized || !stateRotated || !stateDepthReached) {
    m_translationControlOutput = 0;
    m_yawRateControlOutput = 0;
    m_rollRateControlOutput = 0;
    m_pitchRateControlOutput = 0;
  }
}

float m_targetYaw = 0; // this is absolute (we add robot's angle at time when frame was taken to vision output in the vision algorithm)
float m_targetYIntercept = 0; // y-intercept of line that's parallel with long edge of target and has center of target as a point
float m_targetXOffset = 0;
float m_targetYOffset = 0;
float m_targetZOffset = 0;

float m_beaconYaw = 0;
float m_beaconDepth = 0;
bool wreckageFound = false;

void findWreckage() {
  wreckageFound = m_beaconYaw != 0 && m_beaconDepth != 0;

  if (wreckageFound) {
    m_desiredYaw = m_beaconYaw;
    m_desiredDepth = m_beaconDepth + 3; // tune this for vertical offset we want from the wreckage as we approach
    m_translationControlOutput = 0.3; // tune this for how fast we translate once we're locked on
  } else {
    m_desiredYaw += 0.5; // tune this for how fast you turn to search for the target
    m_desiredYaw = fmod((360.0 - m_desiredYaw), 360.0);

    m_desiredDepth = 2; // tune this for what depth you want to be at when you search for wreckage
    m_translationControlOutput = 0;
  }

  stabilize();
  rotate();
  goToDepth();
  translate();
}

bool targetFound;
bool lateralAligned = false;
bool secondStageReached = true;
bool secondStageYawAligned = false;

const float lateralTolerance = 0.5;
const float kVisionTranslationP = 0.04;

const float kVisionYawTolerance = 1;

void alignWithTarget() {
  targetFound = m_targetYaw != 0 && m_targetYIntercept != 0;

  if (targetFound) {
    m_desiredDepth = m_targetZOffset + 5; // tune this for vertical ofset we want from the apriltag as we attempt to align
    m_desiredYaw = m_measuredYaw; // deactivate yaw controller
    if (isDepthReached(m_desiredDepth)) {
      if (abs(m_targetYIntercept) < lateralTolerance) {
        lateralAligned = true; // a one-time toggle
      }
      if (!lateralAligned) {
        m_translationControlOutput = kVisionTranslationP * m_targetYIntercept; // hit point where a turn would make us aligned with y-axis
      } else {
        m_desiredYaw = m_targetYaw; // the turn that makes us aligned with y-axis
        secondStageReached = true;
      }
      if (secondStageReached) {
        if (isAngleAligned(m_measuredYaw, m_targetYaw, kVisionYawTolerance)) {
          m_translationControlOutput = kVisionTranslationP * m_targetYOffset;
        }
      }
    }
    stabilize();
    rotate();
    goToDepth();
    translate();
  }
}

/**
 * Straight lines back and forth
 */
void linePath() {
  long timeElapsed = m_timestamp % 20000;
  if (timeElapsed < 10000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.3;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 10000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = -0.2;
    }
    m_yawControlOutput = 0;
  }
  stabilize();
  rotate();
  goToDepth();
  translate();
}

/**
 * 90-degree turns in place
 */
void nineties() {
  long timeElapsed = m_timestamp % 40000;
  if (timeElapsed < 10000) {
    m_desiredYaw = -90;
  } else if (timeElapsed >= 10000 && timeElapsed < 20000) {
    m_desiredYaw = 0;
  } else if (timeElapsed >= 20000 && timeElapsed < 30000) {
    m_desiredYaw = 90;
  } else if (timeElapsed >= 30000) {
    m_desiredYaw = 0;
  }
  stabilize();
  rotate();
  goToDepth();
  translate();
}

/**
 * Straight lines at varying small speeds
 */
void linePathSlow() {
  long timeElapsed = m_timestamp % 40000;
  if (timeElapsed < 10000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.15;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 10000 && timeElapsed < 20000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = -0.1;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 20000 && timeElapsed < 30000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.1;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 30000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = -0.07;
    }
    m_yawControlOutput = 0;
  }
  stabilize();
  rotate();
  goToDepth();
  translate();
}

/**
 * yuh yeet
 */
void squarePath() {
  long timeElapsed = m_timestamp % 90000;
  if (timeElapsed < 10000) {
    m_desiredYaw = 0;
  } else if (timeElapsed >= 10000 && timeElapsed < 20000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.2;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 20000 && timeElapsed < 30000) {
    m_desiredYaw = -90;
  } else if (timeElapsed >= 30000 && timeElapsed < 40000) {
     if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.2;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 40000 && timeElapsed < 43000) {
    m_desiredYaw = -130;
  } else if (timeElapsed >= 43000 && timeElapsed < 50000) {
    m_desiredYaw = -179;
  } else if (timeElapsed >= 50000 && timeElapsed < 60000) {
     if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.2;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 60000 && timeElapsed < 70000) {
    m_desiredYaw = 90;
  } else if (timeElapsed >= 70000 && timeElapsed < 80000) {
    if (!stateStabilized) {
      m_translationControlOutput = 0;
    } else {
      m_translationControlOutput = 0.2;
    }
    m_yawControlOutput = 0;
  } else if (timeElapsed >= 80000) {
    m_desiredYaw = 0;
  }
  stabilize();
  rotate();
  goToDepth();
  translate();
}

// ======================================================================================= //
//                                                                     END OF AUTO METHODS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                              START OF SERIAL OPERATIONS //
// ======================================================================================= //

// direct input examples
// hL:0.2&hR:0.2&vFL:0.2&vFR:0.2&vBL:0.2&vBR:0.2
// hL:0&hR:0&vFL:0&vFR:0&vBL:0&vBR:0
// cmdYaw:0&cmdPitch:0&cmdRoll:0

#define INPUT_SIZE 30

void receiveSerial() {
  // Get next command from Serial (add 1 for final 0)
  char input[INPUT_SIZE + 1];
  byte size = Serial.readBytes(input, INPUT_SIZE);
  // Add the final 0 to end the C string
  input[size] = 0;

  // Read each command pair
  int index = 0;
  char* command = strtok(input, "&");
  while (command != 0) {
    // Split the command in two values
    char* separator = strchr(command, ':');
    if (separator != 0)
    {
      // Actually split the string in 2: replace ':' with 0
      *separator = 0;
      const char* commandType = command;
      ++separator;
      float input = atof(separator);

      // direct commands
      if (strcmp(commandType, "hL") == 0) {
        directInputArray[0] = input;
      } else if (strcmp(commandType, "hR") == 0) {
        directInputArray[1] = input;
      } else if (strcmp(commandType, "vFL") == 0) {
        directInputArray[2] = input;
      } else if (strcmp(commandType, "vFR") == 0) {
        directInputArray[3] = input;
      } else if (strcmp(commandType, "vBL") == 0) {
        directInputArray[4] = input;
      } else if (strcmp(commandType, "vBR") == 0) {
        directInputArray[5] = input;
      } else if (strcmp(commandType, "cmdYaw") == 0) {
        m_desiredYaw = input;
      } else if (strcmp(commandType, "cmdPitch") == 0) {
        m_desiredPitch = input;
      } else if (strcmp(commandType, "cmdRoll") == 0) {
        m_desiredRoll = input;
      } else if (strcmp(commandType, "cmdDepth") == 0) {
        m_desiredDepth = input;
      } else if (strcmp(commandType, "cmdTrans") == 0) {
        m_translationError = input;
      } 
      // from vision
      else if (strcmp(commandType, "tagYaw") == 0) {
        m_targetYaw = input;
      } else if (strcmp(commandType, "tagYInt") == 0) {
        m_targetYIntercept = input;
      } else if (strcmp(commandType, "tagXOffset") == 0) {
        m_targetXOffset = input;
      } else if (strcmp(commandType, "tagYOffset") == 0) {
        m_targetYOffset = input;
      } else if (strcmp(commandType, "tagZOffset") == 0) {
        m_targetZOffset = input;
      } else if (strcmp(commandType, "beaconYaw") == 0) {
        m_beaconYaw = input; 
      } else if (strcmp(commandType, "beaconDepth") == 0) {
        m_beaconDepth = input;
      } else if (strcmp(commandType, "setAllMotors") == 0) {
        directInputArray[0] = input;
        directInputArray[1] = input;
        directInputArray[2] = input;
        directInputArray[3] = input;
        directInputArray[4] = input;
        directInputArray[5] = input;
      } else if (strcmp(commandType, "stop") == 0) {
        directInputArray[0] = 0;
        directInputArray[1] = 0;
        directInputArray[2] = 0;
        directInputArray[3] = 0;
        directInputArray[4] = 0;
        directInputArray[5] = 0;
      }
    }
    // Find the next command in input string
    command = strtok(0, "&");
  }
}

void sendSerial() {
  Serial.print(m_measuredYaw);
  Serial.print("#");
  Serial.print(m_measuredPitch);
  Serial.print("#");
  Serial.print(m_measuredRoll);
}

void displayStatesToSerial() {
  Serial.println("-----------");
  Serial.print("hL : ");
  Serial.println(m_horizontalLeftPower);
  Serial.print("hR : ");
  Serial.println(m_horizontalRightPower);
  Serial.print("vFL : ");
  Serial.println(m_verticalFrontLeftPower);
  Serial.print("vFR : ");
  Serial.println(m_verticalFrontRightPower);
  Serial.print("vBL : ");
  Serial.println(m_verticalBackLeftPower);
  Serial.print("vBR : ");
  Serial.println(m_verticalBackRightPower);
  Serial.println("");

  Serial.print("Yaw Measured: ");
  Serial.println(m_measuredYaw);
  Serial.print("Roll Measured: " );
  Serial.println(m_measuredRoll);
  Serial.print("Pitch Measured: " );
  Serial.println(m_measuredPitch);
  
  Serial.print("Depth Measured: " );
  Serial.println(m_measuredDepth);
  Serial.print("Altitude Measured: " );
  Serial.println(m_measuredAltitude);
  Serial.print("Temperature Measured: " );
  Serial.println(m_measuredTemperature);

  Serial.print("Is roll aligned: ");
  Serial.println(stateRolled);

  Serial.print("Yaw Desired: ");
  Serial.println(m_desiredYaw);
  Serial.print("Roll Desired: " );
  Serial.println(m_desiredRoll);
  Serial.print("Pitch Desired: " );
  Serial.println(m_desiredPitch);
  Serial.print("Depth Desired: " );
  Serial.println(m_desiredDepth);

  Serial.print("Yaw Error: ");
  Serial.println(m_yawError);
  Serial.print("Roll Error: " );
  Serial.println(m_rollError);
  Serial.print("Pitch Error: " );
  Serial.println(m_pitchError);
  Serial.print("Depth Error: " );
  Serial.println(m_depthError);
  
  Serial.print("Yaw Control Output: ");
  Serial.println(m_yawControlOutput);
  Serial.print("Roll Control Output: " );
  Serial.println(m_rollControlOutput);
  Serial.print("Pitch Control Output: " );
  Serial.println(m_pitchControlOutput);
  Serial.print("Depth Control Output: " );
  Serial.println(m_depthControlOutput);
  Serial.println("-----------");
}

// ======================================================================================= //
//                                                                END OF SERIAL OPERATIONS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                                      START OF MAIN CODE //
// ======================================================================================= //

void setup() {
  Serial.begin(9600);

  instantiateMotors();
  instantiateIMU();
  instantiateBarometer();

  m_startTime = millis();
  m_previousTime = millis();
}

void loop() {
  m_timestamp = millis();
  m_deltaTime = m_timestamp - m_previousTime;
  m_previousTime = m_timestamp;
  
  m_totalTimeElapsed = m_timestamp - m_startTime;
  
  receiveSerial();

  updateStateEstimation();
  calculateControlOutputs();
  displayStatesToSerial();

// modify the block below for different actions -----------------------------

  squarePath();

// --------------------------------------------------------------------------

//  directMotorControl(); // direct serial input to motors
  autonomousControl();  // autonomous update input to motors
  runMotors(); // actuate motors

  sendSerial();

  delay(LOOP_TIME_DELAY_MS);
}

// ======================================================================================= //
//                                                                       #blueteambestteam //
// ======================================================================================= //
