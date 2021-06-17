#include "StepPlanner.h"

StepPlanner::StepPlanner(LegID legID) {
  _legID = legID;
};

/*!
 *    @brief Initializes the gaits and sets up the leg modes
*/
void StepPlanner::init(int16_t robotHeight) {

  _legMode = STANDING;
  _gaitType = DEFAULT_GAIT;
  _robotHeight = robotHeight;

  _gaits[TROT].amplitude = 50;
  _gaits[TROT].periodHalf = 140;

  reset();
}

/*!
 *    @brief Sets the walking giat. Should only be called when the robot is standing
*/
void StepPlanner::setGait(GaitType gaitType) {
  if (_legMode == STANDING)
    _gaitType = gaitType;
}

/*!
 *    @brief Updates the position of the foot as the step progresses.
 *    @param robotMode The mode the robot is in i.e. walking, static_standing.
             used to determine when the first step and the last step are being made
 *    @returns True when the foot position was updated, false if it wasn't.
*/
bool StepPlanner::update(ROBOT_MODE robotMode) {

  // Make sure that the leg needs to stand AND the robot should stand. 
  if ((_legMode == STANDING) && (robotMode == WALKING)) {
    _previousUpdateTime = (millis() - 1);

    /// Determine which foot goes first
    if (_legID == LEG_1 || _legID == LEG_3) {
      _legMode = FIRST_STEP_ARC;
      _footXYDrop  = 0;
    }
    else if (_legID == LEG_2 || _legID == LEG_4) {
      _legMode = FIRST_STEP_DRAW_BACK;
      _footXYDrop  = 0;
    }
    return true;
  }

  float periodHalf = _gaits[_gaitType].periodHalf;

  if ((millis() - _previousUpdateTime) % TIME_TO_UPDATE == 0) {

    // For legs 2 and 3, the negative and positive parts of the x axis are flipped
    if (_legID == LEG_2 || _legID == LEG_3)
      dynamicFootPosition.x *= -1;

    dynamicFootPosition.x = footPosX.update();
    dynamicFootPosition.y = footPosY.update();
    dynamicFootPosition.z = getStepHeight(_footXYDrop, _legMode);

    switch (_legMode) {
      case FIRST_STEP_ARC:            
        _footXYDrop += GAIT_POSITION_INCREMENT; 
        if (_footXYDrop == (periodHalf/2)) {
          _legMode = ACTIVE_WALKING_DRAW_BACK;
        }
        break;
      case FIRST_STEP_DRAW_BACK:      
        _footXYDrop -= GAIT_POSITION_INCREMENT; 
        if (_footXYDrop == -1*(periodHalf/2)) {
          _legMode = ACTIVE_WALKING_ARC;
        }
        break;
      case ACTIVE_WALKING_ARC:
        _footXYDrop += GAIT_POSITION_INCREMENT;
        if (_footXYDrop == (periodHalf/2)) {
          _legMode = ACTIVE_WALKING_DRAW_BACK;
        }
        break;
      case ACTIVE_WALKING_DRAW_BACK:
        _footXYDrop -= GAIT_POSITION_INCREMENT;
        if (_footXYDrop == -1*(periodHalf/2)) {
          _legMode = ACTIVE_WALKING_ARC;
        }
        break;
      case STANDING:
        break;
    }
    _previousUpdateTime = (millis() - 1);
    return true;
  }
  return false;
};

/*!
 *    @brief Performs the calculation of the arc itself ONLY... doesn't know where the foot actually is
 *    @param footXYDropL The x drop position of the foot.
 *    @param legMode The mode/phase of walking the leg is in.
 *    @returns The hight that should be written to the legs i.e. foot z distance (foot-should) - curve hight at the given footXYDropL
*/
int16_t StepPlanner::getStepHeight(int16_t footXYDropL, LegMode legMode) {

  int16_t stepHeight = 0;

  float periodHalf = _gaits[_gaitType].periodHalf;
  float amplitude = _gaits[_gaitType].amplitude;

  switch (legMode) {
    case FIRST_STEP_ARC:           stepHeight = _robotHeight - lrint( (amplitude/2) * cos(PI * (footXYDropL - (periodHalf/4))/(periodHalf/2) ) ); break;
    case FIRST_STEP_DRAW_BACK:     stepHeight = _robotHeight - 0; break;
    case ACTIVE_WALKING_ARC:       stepHeight = _robotHeight - lrint( amplitude * cos( (PI * (footXYDropL)/periodHalf) ) ); break;
    case ACTIVE_WALKING_DRAW_BACK: stepHeight = _robotHeight - 0; break;
    case STANDING:                 stepHeight = _robotHeight - 0;
  }

  return stepHeight;

};

/*!
 *    @brief Allows you to set the endpoint of the step depending on the direction you command
 *    @param controllCoordinateX X direction of the controller coordinate 
 *    @param controllCoordinateY Y direction of the controller coordinate 
 *    @returns True if it's time to update the endpoint, false if it's not.
*/
void StepPlanner::setStepEndpoint(int16_t controlCoordinateX, int16_t controlCoordinateY) {

  float stepEndpointX = 0.0;
  float stepEndpointY = 0.0;

  float periodHalf = _gaits[_gaitType].periodHalf;
  float movementGradient = 0;

  // Check if stopped walking
  if (controlCoordinateX == 0 && controlCoordinateY == 0) {
    _stepEndpoint.x = 0.0;
    _stepEndpoint.y = 0.0;
    footPosX.go(_stepEndpoint.x);
    footPosY.go(_stepEndpoint.y);
    _legMode = STANDING;
    return;
  }

  // make sure that math works (vertical lines are undefined)
  else if (controlCoordinateX == 0) {
    stepEndpointX = 0;
    stepEndpointY = periodHalf/2;
  }
  else {
    movementGradient = (controlCoordinateY / controlCoordinateX);

    stepEndpointX = ((periodHalf/2) / sqrt(1 + pow(movementGradient, 2)));
    stepEndpointY = (((periodHalf/2) * abs(movementGradient)) / sqrt(1 + pow(movementGradient, 2)));
  }

  if (controlCoordinateX < 0)
    stepEndpointX *= -1;
  if (controlCoordinateY < 0)
    stepEndpointY *= -1;
  // The formula for the y axis preserves the sign of movementGradient! 

  if ((_legMode == ACTIVE_WALKING_DRAW_BACK) || (_legMode == FIRST_STEP_DRAW_BACK)) {
    stepEndpointX *= -1;
    stepEndpointY *= -1;
  }

  // flip the result (kinematics thinks that x is moving forwards/backwards while looking down the robot)
  // stepPlanner thinks that y is moving forwards/backwards
  _stepEndpoint.x = stepEndpointY;
  _stepEndpoint.y = stepEndpointX;

  long completionTime = (long)((TIME_TO_UPDATE - 1) * (periodHalf/(2 * GAIT_POSITION_INCREMENT)));

  // Ideally, the interpolation objects will effectively track the position of footXY drop.
  // When it reaches its maximum point, so does footXY drop (likewise when they are 0).

  footPosX.go(_stepEndpoint.x, completionTime, LINEAR, FORTHANDBACK);
  footPosY.go(_stepEndpoint.y, completionTime, LINEAR, FORTHANDBACK);
};


/*!
 *    @brief Used to figure out if it's time to update the step endpoint
 *    @returns True if it's time to update the endpoint, false if it's not.
*/
bool StepPlanner::footAtOrigin() {
  if ((_footXYDrop == 0) && (_wasAtOrigin == false)) {
    _wasAtOrigin = true;
    return true;
  }
  if ((_footXYDrop != 0))
    _wasAtOrigin = false;
  return false;
}

/*!
 *    @brief Resets all dynamic gait parameters
*/
void StepPlanner::reset() {
  dynamicFootPosition.x = 0;
  dynamicFootPosition.y = 0;
  dynamicFootPosition.z = _robotHeight;

  _wasAtOrigin = false;
  _legMode = STANDING;
  _footXYDrop = 0;

  footPosX.go(0);
  footPosY.go(0);
}