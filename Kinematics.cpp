#include "Kinematics.h"

#include <Arduino.h>

Kinematics::Kinematics(uint8_t legID, int16_t inputX, int16_t inputY, int16_t inputZ, uint16_t motor1CalibOffset, uint16_t motor2CalibOffset, uint16_t motor3CalibOffset) {
  _legID = legID;


  // This solves for the motor angles in degrees and micros
  solveFootPosition(inputX, inputY, inputZ, &motor1.angleDegrees, &motor2.angleDegrees, &motor3.angleDegrees);

  motor1.angleMicros = _degreesToMicros(motor1.angleDegrees, motor1CalibOffset);
  motor1.angleDegrees = motor1.dynamicDegrees;
  motor1.dynamicMicros = _degreesToMicros(motor1.dynamicDegrees, motor1CalibOffset);
  motor1.previousDegrees = 360;         // 360 is a magic number. It just must be different than the start positions so that a call to updateDynamicPositions() works
  motor1.calibOffset = motor1CalibOffset;

  motor2.angleMicros = _degreesToMicros(motor2.angleDegrees, motor2CalibOffset);
  motor2.angleDegrees = motor2.dynamicDegrees;
  motor2.dynamicMicros = _degreesToMicros(motor2.dynamicDegrees, motor2CalibOffset);
  motor2.previousDegrees = 360;         // 360 is a magic number. It just must be different than the start positions so that a call to updateDynamicPositions() works
  motor2.calibOffset = motor2CalibOffset;

  motor3.angleMicros = _degreesToMicros(motor3.angleDegrees, motor3CalibOffset);
  motor3.angleDegrees = motor3.dynamicDegrees;
  motor3.dynamicMicros = _degreesToMicros(motor3.dynamicDegrees, motor3CalibOffset);
  motor3.previousDegrees = 360;         // 360 is a magic number. It just must be different than the start positions so that a call to updateDynamicPositions() works
  motor3.calibOffset = motor3CalibOffset;

  dynamicX.go(inputX);
  dynamicY.go(inputY);
  dynamicZ.go(inputZ);

};


// *****************Private Functions*****************

uint16_t Kinematics::_degreesToMicros(uint8_t inputDegrees, uint8_t calibOffset) {
  int microsecondsInput = ((DEGREES_TO_MICROS * inputDegrees) + 500 + calibOffset);    // 500 is a "magic number" of micros for the motors; before that they do nothing
  return microsecondsInput;
};


// *****************Public Functions*****************

void Kinematics::setFootEndpoint(int16_t inputX, int16_t inputY, int16_t inputZ) {

  solveFootPosition(inputX, inputY, inputZ, &motor1.angleDegrees, &motor2.angleDegrees, &motor3.angleDegrees);

  uint16_t motor1AngleDelta = abs(motor1.angleDegrees - motor1.previousDegrees);
  uint16_t motor2AngleDelta = abs(motor2.angleDegrees - motor2.previousDegrees);
  uint16_t motor3AngleDelta = abs(motor3.angleDegrees - motor3.previousDegrees);
  uint16_t demandTime = lrint(MAX_SPEED_INVERSE * max(max(motor1AngleDelta, motor2AngleDelta), motor3AngleDelta));


    // determine whether motor angles have been updated i.e. new end angle, and update final positions accordingly
  if ((motor1.previousDegrees != motor1.angleDegrees) || (motor2.previousDegrees != motor2.angleDegrees) || (motor3.previousDegrees != motor3.angleDegrees)) {
    motor1.previousDegrees = motor1.angleDegrees;
    motor2.previousDegrees = motor2.angleDegrees;
    motor3.previousDegrees = motor3.angleDegrees;

    dynamicX.go(inputX, demandTime, LINEAR, ONCEFORWARD);
    dynamicY.go(inputY, demandTime, LINEAR, ONCEFORWARD);
    dynamicZ.go(inputZ, demandTime, LINEAR, ONCEFORWARD);
  }
}

void Kinematics::updateDynamicFootPosition() {

  solveFootPosition(dynamicX.update(), dynamicY.update(), dynamicZ.update(), &motor1.dynamicDegrees, &motor2.dynamicDegrees, &motor3.dynamicDegrees);
  motor1.dynamicMicros = _degreesToMicros(motor1.dynamicDegrees, motor1.calibOffset);
  motor2.dynamicMicros = _degreesToMicros(motor2.dynamicDegrees, motor2.calibOffset);
  motor3.dynamicMicros = _degreesToMicros(motor3.dynamicDegrees, motor3.calibOffset);
}


void Kinematics::solveFtShldrLength(float demandFtShldr, float *demandAngle2, float *demandAngle3) {
  
  float _demandFtShldrLength = demandFtShldr;
  if (_demandFtShldrLength > SHOULDER_FOOT_MAX) 
    _demandFtShldrLength = SHOULDER_FOOT_MAX;
  else if (_demandFtShldrLength < SHOULDER_FOOT_MIN)
    _demandFtShldrLength = SHOULDER_FOOT_MIN;

  // Use the Law of Cosines to solve for the angles of motor 3 and convert to degrees
  float _demandAngle3 = acos( ( pow(demandFtShldr, 2) - pow(LIMB_2, 2) - pow(LIMB_3, 2) ) / (-2 * LIMB_2 * LIMB_3) ); // demand angle for position 3 (operated by M3)
  _demandAngle3 = ((_demandAngle3 * 180) / PI);   //convert to degrees

  // Use demandAngle3 to calculate for demandAngle2 (angle for M2)
  float _demandAngle2 = ((180 - _demandAngle3) / 2 );

  *demandAngle2 += _demandAngle2;
  *demandAngle3 += _demandAngle3;
};



void  Kinematics::solveXMove(int16_t inputX, int16_t inputZ, float *demandAngle2, float *demandFtShldrLength) {
  if (inputZ == 0)
    inputZ = 1;   // you can never divide by 0!

  *demandFtShldrLength = sqrt(pow((float)abs(inputZ), 2) + pow((float)abs(inputX), 2));

  *demandAngle2 = ((atan((float)abs(inputX)/(float)abs(inputZ))*180) / PI);

  if (inputX > 0)
    *demandAngle2 *= -1;            // change later: make it negative if inputX is in the negative direction and parse it later
};



void Kinematics::solveYMove(int16_t inputY, int16_t inputZ, float *demandAngle1, float *yPlaneZOutput) {
  float demandFtShldrLength = sqrt(pow((float)abs(inputZ), 2) + pow((float)abs(inputY), 2)); // foot-shoulder distance on y-z plane (L1 in diagram)
  *yPlaneZOutput = sqrt(pow((float)abs(demandFtShldrLength), 2) - pow((float)abs(LIMB_1), 2));

  // Here, theta is the angle closest to the axis of rotation in the triangle relating inputY and inputZ
  // Alpha is the angle closest to the axis of rotation in the triangle relating leg length output to LIMB_1 length
  float theta = (float)abs((((float)atan((float)inputY/(float)inputZ) * 180) / PI));
  float alpha = (float)(((float)acos((float)LIMB_1/demandFtShldrLength) * 180) / PI);
  if (inputY >= 0) {
    *demandAngle1 += (float)abs((float)90 - (theta + alpha));
  }
  else if (inputY < 0) {
    *demandAngle1 += (float)abs((float)90 - (alpha - theta));   // since both triangles (refer to drawings) have the same hypotenuse, alpha > theta for all inputY
  }

  if (inputY < LIMB_1)
    *demandAngle1 *= -1;

}


void Kinematics::solveFootPosition(int16_t inputX, int16_t inputY, int16_t inputZ, uint16_t *motor1AngleP, uint16_t *motor2AngleP, uint16_t *motor3AngleP) {
  float demandAngle1 = 0;
  float demandAngle2 = 0;
  float demandAngle3 = 0;

  float yPlaneZOutput = 0;  // this is the foot-shoulder distance on the y-z plane (L1 in diagram), and the distance the leg must stretch to achieve the desired y movement on the y-z plane. 
  float demandFtShldrLength = 0;  // this is the foot-should distance on the x-z plane and the final calculated length

  solveYMove(inputY, inputZ, &demandAngle1, &yPlaneZOutput);

  solveXMove(inputX, yPlaneZOutput, &demandAngle2, &demandFtShldrLength);

  solveFtShldrLength(demandFtShldrLength, &demandAngle2, &demandAngle3);

  // Round off demand angles
  demandAngle1 = lrint(demandAngle1);
  demandAngle2 = lrint(demandAngle2);
  demandAngle3 = lrint(demandAngle3);

  // Calculate final demand angles suited to motors by applying necessary offsets
  demandAngle1 += M1_OFFSET;
  demandAngle2 += M2_OFFSET;
  demandAngle3 = (M3_OFFSET - demandAngle3) + M3_OFFSET;

  // Constrain motor angles
  demandAngle1 = _applyConstraints(1, demandAngle1);
  demandAngle2 = _applyConstraints(2, demandAngle2);
  demandAngle3 = _applyConstraints(3, demandAngle3);

  // Set live motor angles to the newly calculated ones

  //motor 1: 
  *motor1AngleP = demandAngle1; // In degrees!

  // motor 2:
  *motor2AngleP = demandAngle2; // In degrees!

  // motor 3:
  *motor3AngleP = demandAngle3; // In degrees!
};



float Kinematics::_applyConstraints(uint8_t motor, float demandAngle) {
  if (motor == 1) {
    if (demandAngle > M1_MAX){
      return M1_MAX;
    }
    else if (demandAngle < M1_MIN){
      return M1_MIN;
    }
    else
      return demandAngle;
  }
  else if (motor == 2) {
    if (demandAngle > M2_MAX){
      return M2_MAX;
    }
    else if (demandAngle < M2_MIN){
      return M2_MIN;
    }
    else
      return demandAngle;
  }
  else if (motor == 3) {
    if (demandAngle > M3_MAX) {
      return M3_MAX;
    }
    else if (demandAngle < M3_MIN) {
      return M3_MIN;
    }
    else 
      return demandAngle;
  }
  else {
    if (Serial)
      Serial.println("Motor argument in _apply constraints wrong, so constraints can't be applied! Terminating program."); //need a better way to report error
    while(1);         // terminate the program to avoid unpredictable movement (which could break stuff)
    return demandAngle;
  }
};