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

long m_previousTime = 0;
long m_deltaTime = 0;

const float kYawP = 0.005;
const float kYawI = 0;
const float kYawD = 0;
const float kPitchP = 0.005;
const float kPitchI = 0;
const float kPitchD = 0;
const float kRollP = 0.005;
const float kRollI = 0;
const float kRollD = 0;
const float kDepthP = 0;
const float kDepthI = 0;
const float kDepthD = 0;

const float kYawRateP = 0;
const float kPitchRateP = 0;
const float kRollRateP = 0;

const float kTranslationP = 0;
const float kTranslationI = 0;
const float kTranslationD = 0;

const float kYawThreshold = 10;
const float kPitchThreshold = 10;
const float kRollThreshold = 10;
const float kDepthThreshold = 5;

float universalMaxPower = 0.7;

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

void instantiateMotors() {
  Serial.println("MOTORS INSTANTIATING");
  
  m_horizontalLeftMotor.attach(11);
  m_horizontalRightMotor.attach(6);
  m_verticalFrontLeftMotor.attach(3);
  m_verticalFrontRightMotor.attach(10);
  m_verticalBackLeftMotor.attach(9);
  m_verticalBackRightMotor.attach(5);
  
  stopAll();
  delay(10000);
  
  Serial.println("MOTORS INSTANTIATED");
}

void instantiateIMU() {
  Serial.println("IMU INSTANTIATING");
  
  m_imu = Adafruit_BNO055();
  while(!m_imu.begin())
  {
    Serial.println("IMU INIT FAILED");
    delay(5000);
  }
  delay(5000);
  
  Serial.println("IMU INSTANTIATED");
}

void instantiateBarometer() {
  Serial.println("BAROMETER INSTANTIATING");
  
  Wire.begin();
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
  m_measuredYaw = euler.x();
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
  m_measuredDepth = m_barometer.depth() / 3.28084;
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

bool isYawAligned = false;
bool isRollAligned = false;
bool isPitchAligned = false;
bool isDepthReached = false;

float m_yawFromVisionB = 0;
float m_pitchFromVisionB = 0;
float m_rollFromVisionB = 0;
float m_transXFromVisionB = 0;
float m_transYFromVisionB = 0;
float m_transZFromVisionB = 0;

float m_yawFromVisionF = 0;
float m_pitchFromVisionF = 0;
float m_rollFromVisionF = 0;
float m_transXFromVisionF = 0;
float m_transYFromVisionF = 0;
float m_transZFromVisionF = 0;

float m_yawFromVisionR = 0;
float m_pitchFromVisionR = 0;
float m_rollFromVisionR = 0;
float m_transXFromVisionR = 0;
float m_transYFromVisionR = 0;
float m_transZFromVisionR = 0;

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

  m_yawControlOutput = kYawP * m_yawError;
  m_yawControlOutput *= abs(m_yawControlOutput);
  m_pitchControlOutput = kPitchP * m_pitchError;
  m_rollControlOutput = kRollP * m_rollError;

//  m_yawControlOutput = kYawP * m_yawError - kYawD * ((m_yawError - m_lastYawError) / m_deltaTime);
//  m_pitchControlOutput = kPitchP * m_pitchError - kPitchD * ((m_pitchError - m_lastPitchError) / m_deltaTime);
//  m_rollControlOutput = kRollP * m_rollError - kRollD * ((m_rollError - m_lastRollError) / m_deltaTime);

  m_yawRateControlOutput = kYawRateP * -m_measuredYawRate;
  m_pitchRateControlOutput = kPitchRateP * -m_measuredPitchRate;
  m_rollRateControlOutput = kRollRateP * -m_measuredRollRate;

  m_depthControlOutput = kDepthP * m_depthError;
  m_translationControlOutput = kTranslationP * m_translationError;

  isYawAligned = abs(m_yawError) < kYawThreshold;
  isRollAligned = abs(m_rollError) < kRollThreshold;
  isPitchAligned = abs(m_pitchError) < kPitchThreshold;
  isDepthReached = abs(m_depthError) < kDepthThreshold;
}

/**
 * Limit control outputs in effect
 */
void rotate() {
//  if (!isRollAligned) {
//    m_pitchControlOutput = 0;
//    m_yawControlOutput = 0;
//  } else if (!isPitchAligned) {
//    m_yawControlOutput = 0;
//  }
//
//  m_yawRateControlOutput = 0;
//  m_rollRateControlOutput = 0;
//  m_pitchRateControlOutput = 0;

  //tuning individual
//  m_rollControlOutput = kRollP * m_rollError;
  m_rollControlOutput = 0;
//  m_pitchControlOutput = kPitchP * m_pitchError;
  m_pitchControlOutput = 0;
//  m_yawControlOutput = kYawP * m_yawError;
//  m_yawControlOutput = 0;

  m_depthControlOutput = 0;
}

/**
 * Go to depth
 */
void goToDepth() {
//  m_desiredPitch = 0;
//  m_desiredRoll = 0;
//  rotate();
//
//  if (!isPitchAligned || !isRollAligned) {
//    m_depthControlOutput = 0;
//  }

  m_yawControlOutput = 0;
  m_rollControlOutput = 0;
  m_pitchControlOutput = 0;
}

/**
 * Translate to based on error input (m_translationError) given rotation is aligned
 */
void translate() {
  if (!isYawAligned || !isPitchAligned || !isRollAligned || !isDepthReached) {
    m_translationControlOutput = 0;
  }
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
//  if (throttle > universalMaxPower) {
//    throttle = universalMaxPower;
//  } else if (throttle < -universalMaxPower) {
//    throttle = -universalMaxPower;
//  }
  float input = throttle * 400 + 1500;
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

//  m_horizontalLeftPower = 0;
//  m_horizontalRightPower = 0;
//  m_verticalFrontLeftPower = 0;
//  m_verticalFrontRightPower = 0;
//  m_verticalBackLeftPower = 0;
//  m_verticalBackRightPower = 0;
}

/**
 * Autonomous motor control (all control outputs added)
 */
void autonomousControl() {
  m_horizontalLeftPower = m_yawControlOutput + m_translationControlOutput;
  m_horizontalRightPower = -m_yawControlOutput + m_translationControlOutput;
//  m_verticalFrontLeftPower = 0;
//  m_verticalFrontRightPower = 0;
//  m_verticalBackLeftPower = 0;
//  m_verticalBackRightPower = 0;

//  m_horizontalLeftPower = 0;
//  m_horizontalRightPower = 0;
  m_verticalFrontLeftPower = -m_rollControlOutput + m_pitchControlOutput + m_depthControlOutput;
  m_verticalFrontRightPower = m_rollControlOutput + m_pitchControlOutput + m_depthControlOutput;
  m_verticalBackLeftPower = -m_rollControlOutput - m_pitchControlOutput + m_depthControlOutput;
  m_verticalBackRightPower = m_rollControlOutput - m_pitchControlOutput + m_depthControlOutput;
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
  m_verticalFrontLeftMotor.writeMicroseconds(throttleToMicroseconds(-m_verticalFrontLeftPower));
  m_verticalFrontRightMotor.writeMicroseconds(throttleToMicroseconds(m_verticalFrontRightPower));
  m_verticalBackLeftMotor.writeMicroseconds(throttleToMicroseconds(m_verticalBackLeftPower));
  m_verticalBackRightMotor.writeMicroseconds(throttleToMicroseconds(-m_verticalBackRightPower));
}

// ======================================================================================= //
//                                                                 END OF MOVEMENT METHODS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                                   START OF AUTO METHODS //
// ======================================================================================= //

//int state = 0;
//
//double calculateInitialYaw() {
//  return atan2(m_transXFromVisionF, m_transZFromVisionF);
//}
//
//double calculateInitialDepth() {
//  return m_transYFromVisionF;
//}
//
//bool start = false;
//bool wreckageFound = false;
//float m_subDesiredYaw = 0;
//
//void findWreckage() {
//  universalMaxPower = 0.5;
//  
//  if (!start)  {
//    m_subDesiredYaw = m_measuredYaw;
//    start = true;
//  }
//  
//  m_desiredRoll = 0;
//  m_desiredPitch = 0;
//  m_subDesiredYaw += 0.5; // tune this number to control how fast the Meg turns
//  
//  m_desiredYaw = fmod((360.0 - m_subDesiredYaw), 360.0);
//
//  m_desiredDepth = kDesiredDepthForWreckageLocation;
//  if (isYawAligned && isPitchAligned && isRollAligned && isDepthReached) {
//    translate();
//  } else {
//    rotate();
//    if (isPitchAligned && isRollAligned && !isDepthReached) {
//      goToDepth();
//    }
//  }
//
//  if (m_yawFromVisionF != 0) {
//    wreckageFound = true;
//  }
//  
//  rotate();
//}
//
//void moveToWreckage() {
//  universalMaxPower = 0.5;
//  
//  m_desiredYaw = m_yawFromVisionF; // lets call this horizontal offset from vision later, jk we need to calculate desired yaw in vision algorithm
//  m_desiredDepth = kDesiredDepthForWreckageLocation;
//  
//  if (wreckageFound) {
//    m_translationError = 0;
//  } else {
//    m_translationError = m_transXFromVision;
//  }
//  
//  if (isYawAligned && isPitchAligned && isRollAligned && isDepthReached) {
//    translate();
//  } else {
//    if (!isPitchAligned || !isRollAligned) {
//      rotate();
//    } else if (!isDepthReached) {
//      goToDepth();
//    }
//  }
//}
//
//bool targetAligned = false;
//
//void moveAboveTarget() {
//  universalMaxPower = 0.5;
//  
//  if (visYawB == 0) {
//    translate();
//  } else {
//    m_desiredYaw = visYawB;
//    m_desiredRoll = 0;
//    m_desriredPitch = 0;
//  }
//
//  if (isYawAligned) {
//    targetAligned = true;
//  }
//}
//
//void alignWithTarget() {
//  universalMaxPower = 0.5;
//  
//  if (targetAligned) {
//    if (m_desiredDepth < kMinDepth) {
//      m_desiredDepth -= 0.5; // use this to control how fast the meg sinks
//    } else {
//      m_desiredDepth = kMinDepth;
//      closeClaw();
//      returnToHome();
//    }
//  }
//}
//
//void returnToHome() {
//  m_desiredRoll = 0;
//  m_desiredPitch = 0;
//  m_desiredYaw = m_measuredYaw;
//
//  m_desiredDepth = 0;
//  if (!isRollAligned || !isPitchAligned) {
//    rotate();
//    m_desiredDepth = m_measuredDepth;
//  }
//  
//  universalLimitedPower = 0.9;
//}

// ======================================================================================= //
//                                                                     END OF AUTO METHODS //
// ======================================================================================= //
//                                                                                         //
// ======================================================================================= //
//                                                              START OF SERIAL OPERATIONS //
// ======================================================================================= //

// direct input //
// hL:0.2&hR:0.2&vFL:0.2&vFR:0.2&vBL:0.2&vBR:0.2
// hL:0&hR:0&vFL:0&vFR:0&vBL:0&vBR:0
// cmdYaw:0&cmdPitch:0&cmdRoll:0

// autonomous input //

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
      } else if (strcmp(commandType, "visYawB") == 0) {
        m_yawFromVisionB = input;
      } else if (strcmp(commandType, "visRollB") == 0) {
        m_rollFromVisionB = input;
      } else if (strcmp(commandType, "visPitchB") == 0) {
        m_pitchFromVisionB = input;
      } else if (strcmp(commandType, "visXB") == 0) {
        m_transXFromVisionB = input;
      } else if (strcmp(commandType, "visYB") == 0) {
        m_transYFromVisionB = input;
      } else if (strcmp(commandType, "visZB") == 0) {
        m_transZFromVisionB = input;
      } else if (strcmp(commandType, "visYawF") == 0) {
        m_yawFromVisionF = input;
      } else if (strcmp(commandType, "visRollF") == 0) {
        m_rollFromVisionF = input;
      } else if (strcmp(commandType, "visPitchF") == 0) {
        m_pitchFromVisionF = input;
      } else if (strcmp(commandType, "visXF") == 0) {
        m_transXFromVisionF = input;
      } else if (strcmp(commandType, "visYF") == 0) {
        m_transYFromVisionF = input;
      } else if (strcmp(commandType, "visZF") == 0) {
        m_transZFromVisionF = input;
      } else if (strcmp(commandType, "visYawR") == 0) {
        m_yawFromVisionR = input;
      } else if (strcmp(commandType, "visRollR") == 0) {
        m_rollFromVisionR = input;
      } else if (strcmp(commandType, "visPitchR") == 0) {
        m_pitchFromVisionR = input;
      } else if (strcmp(commandType, "visXR") == 0) {
        m_transXFromVisionR = input;
      } else if (strcmp(commandType, "visYR") == 0) {
        m_transYFromVisionR = input;
      } else if (strcmp(commandType, "visZR") == 0) {
        m_transZFromVisionR = input;
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

//void sendSerial() {
//  Serial.print(measured_yaw);
//  Serial.print("#");
//  Serial.print(measured_pitch);
//  Serial.print("#");
//  Serial.print(measured_roll);
//}

void displayStatesToSerial() {
//  Serial.println("-----------");
//  Serial.print("hL : ");
//  Serial.println(m_horizontalLeftPower);
//  Serial.print("hR : ");
//  Serial.println(m_horizontalRightPower);
//  Serial.print("vFL : ");
//  Serial.println(m_verticalFrontLeftPower);
//  Serial.print("vFR : ");
//  Serial.println(m_verticalFrontRightPower);
//  Serial.print("vBL : ");
//  Serial.println(m_verticalBackLeftPower);
//  Serial.print("vBR : ");
//  Serial.println(m_verticalBackRightPower);
//  Serial.println("");

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

//  Serial.print("Yaw Desired: ");
//  Serial.println(m_desiredYaw);
//  Serial.print("Roll Desired: " );
//  Serial.println(m_desiredRoll);
//  Serial.print("Pitch Desired: " );
//  Serial.println(m_desiredPitch);
//  Serial.print("Depth Desired: " );
//  Serial.println(m_desiredDepth);
//
//  Serial.print("Yaw Error: ");
//  Serial.println(m_yawError);
//  Serial.print("Roll Error: " );
//  Serial.println(m_rollError);
//  Serial.print("Pitch Error: " );
//  Serial.println(m_pitchError);
//  Serial.print("Depth Error: " );
//  Serial.println(m_depthError);
//  
//  Serial.print("Yaw Control Output: ");
//  Serial.println(m_yawControlOutput);
//  Serial.print("Roll Control Output: " );
//  Serial.println(m_rollControlOutput);
//  Serial.print("Pitch Control Output: " );
//  Serial.println(m_pitchControlOutput);
//  Serial.print("Depth Control Output: " );
//  Serial.println(m_depthControlOutput);
//  Serial.println("-----------");
}

void simulate() {
  Serial.println("-----------");
  Serial.print("Bottom Camera Yaw: ");
  Serial.println(m_yawFromVisionB);
  Serial.print("Bottom Camera Roll: ");
  Serial.println(m_rollFromVisionB);
  Serial.print("Bottom Camera Pitch: ");
  Serial.println(m_pitchFromVisionB);
  Serial.print("Bottom Camera X Translation: ");
  Serial.println(m_transXFromVisionB);
  Serial.print("Bottom Camera Y Translation: ");
  Serial.println(m_transYFromVisionB);
  Serial.print("Bottom Camera Z Translation: ");
  Serial.println(m_transZFromVisionB);
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
//  instantiateBarometer();

  m_previousTime = millis();
}

void loop() {
//  receiveSerial();
//  simulate();
  long timestamp = millis();
  m_deltaTime = timestamp - m_previousTime;

  updateStateEstimation();
  calculateControlOutputs();
  
//  displayStatesToSerial();
  
//  if (!isDepthReached) {
//    goToDepth();
//  } else if (!isYawAligned && !isRollAligned && !isPitchAligned) {
//    rotate();
//  }

  long tempTimestamp = timestamp % 40000;
  if (tempTimestamp < 10000) {
    m_desiredYaw = -90;
  } else if (tempTimestamp > 10000 && tempTimestamp < 20000) {
    m_desiredYaw = 0;
  } else if (tempTimestamp > 20000 && tempTimestamp < 30000) {
    m_desiredYaw = 90;
  } else if (tempTimestamp > 30000) {
    m_desiredYaw = 0;
  }

  rotate();

//  directMotorControl(); // direct serial input to motors
  autonomousControl();  // autonomous update input to motors
//  runMotors(); // actuate motors

//  sendSerial();
  m_previousTime = timestamp;

  delay(LOOP_TIME_DELAY_MS);
}

// ======================================================================================= //
//                                                                       #blueteambestteam //
// ======================================================================================= //
