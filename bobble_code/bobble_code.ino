// ---------------------------------------------------------------------------
// Created by P2fryang - https://github.com/P2fryang
//
// See the README.md from some ramblings or look down for the math and code
// ---------------------------------------------------------------------------
#include <NewPing.h>
#include <QuickMedianLib.h>
#include <Servo.h>

#define MAX_DIST 180
#define NUM_DIST 4
#define NUM_SENSORS 4
#define NUM_MOTORS 12
#define JITTER_FRAMES 2
#define FRAME_DELAY 24
#define DELTA_RANDOM_RANGE 10

const float MIN_INTERSECT_DIST = 20;
const float NEIGHBOR_MAX_DIST = 5;
const float MOTOR_THRESHOLD = 3;
const float MOTOR_DELTA = 10;

//#define RIGHT 1
//#define RESET_IDLE 1
//#define SERIAL_PRINT 1

//#############################################
//#############################################
//#############################################
//####                                    #####
//####              RIGHT                 #####
//####                                    #####
//#############################################
//#############################################
//#############################################
#ifdef RIGHT
#define X_OFFSET 0
#define Y_OFFSET 0
// As long as the index of the pin and
// position of the motor/sensor match,
// it doesn't matter what order you put it in
const byte motorPins[NUM_MOTORS] = {
  10,
  11,
  12,
  13,
  6,
  7,
  8,
  9,
  2,
  3,
  4,
  5
};
const byte sensorPins[NUM_SENSORS] =  {
  23,
  22,
  25,
  24
};
const float sensorPositions[NUM_SENSORS * 2] =  {
  0, 0,
  15, 0,
  29.8, 0,
  44.8, 0
};
const float motorPositions[NUM_MOTORS * 2] =  {
  -3, -15,
  7, -15,
  17, -15,
  27, -15,
  -3, -32,
  7, -32,
  17, -32,
  27, -32,
  -3, -45,
  7, -45,
  17, -45,
  27, -45
};

#else
//#############################################
//#############################################
//#############################################
//####                                    #####
//####              LEFT                  #####
//####                                    #####
//#############################################
//#############################################
//#############################################
#define X_OFFSET 0
#define Y_OFFSET 0
// As long as the index of the pin and
// position of the motor/sensor match,
// it doesn't matter what order you put it in
const byte motorPins[NUM_MOTORS] = {
  13,
  12,
  11,
  10,
  7,
  6,
  9,
  8,
  2,
  3,
  4,
  5
};
const byte sensorPins[NUM_SENSORS] =  {
  25,
  24,
  22,
  23
};
const float sensorPositions[NUM_SENSORS * 2] =  {
  0, 0,
  15, 0,
  29.8, 0,
  44.8, 0
};
const float motorPositions[NUM_MOTORS * 2] =  {
  15, -16,
  25, -16,
  35, -16,
  45, -16,
  15, -35,
  25, -35,
  36, -35,
  45, -35,
  15, -52,
  25, -52,
  35, -52,
  45, -52
};
#endif

int jitterCounter = 0;
float closestDist = MAX_DIST;
float closestDistN = MAX_DIST;
int closestInd = -1;
int closestIndN = -1;
float sensorDistancesRaw[NUM_SENSORS * NUM_DIST] = {0};
int distP;
float sensorDistances[NUM_SENSORS] = {0};
float motorAngles[NUM_MOTORS] = {0};
Servo motors[NUM_MOTORS];
NewPing sensors[NUM_SENSORS] = {
  NewPing(sensorPins[0], sensorPins[0], MAX_DIST),
  NewPing(sensorPins[1], sensorPins[1], MAX_DIST),
  NewPing(sensorPins[2], sensorPins[2], MAX_DIST),
  NewPing(sensorPins[3], sensorPins[3], MAX_DIST)
};

float targetX, targetY;

//#############################################
//#############################################
//#############################################
//####                                    #####
//####              SETUP                 #####
//####                                    #####
//#############################################
//#############################################
//#############################################
void setup()
{
#ifdef SERIAL_PRINT
  Serial.begin(115200);
#endif

  // Initialize the motors
  for (int i = 0; i < NUM_MOTORS; i++)
  {
    motors[i].attach(motorPins[i]);
    motorAngles[i] = 90;
  }
}

//#############################################
//#############################################
//#############################################
//####                                    #####
//####              SETUP                 #####
//####                                    #####
//#############################################
//#############################################
//#############################################
void loop()
{
  delay(FRAME_DELAY);
  // update sensor readings
  updateSensorReadings();
#ifdef SERIAL_PRINT
  Serial.println();
#endif

  // calculate target coordinate
  calculateTargetCoordinates();

  // per motor, calculate motor angles
  updateMotors();

  // per motor, move motor();
  if (!jitterCounter) {
    moveMotors();
  }
  jitterCounter = (jitterCounter + 1) % JITTER_FRAMES;

}

//#############################################
//#############################################
//#############################################
//####                                    #####
//####             SENSOR                 #####
//####                                    #####
//#############################################
//#############################################
//#############################################

void updateSensorReadings()
{
  // default to no valid findings
  closestDist = MAX_DIST;
  closestDistN = MAX_DIST;
  closestInd = -1;
  closestIndN = -1;

  // update sensor readings
  // potentially flawed 1st iter, but reduce looping
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    // get distance read of sensor "i"
    float currDist = readSensor(i);

#ifdef SERIAL_PRINT
    Serial.print(i);
    Serial.print(" " );
    Serial.print(currDist);
    Serial.print("\t");
#endif

    if (currDist < closestDist)
    {
      // save closest info
      closestDist = currDist;
      closestInd = i;

      // find suitable neighbor
      if (i == 0 && sensorDistances[1] >= MIN_INTERSECT_DIST)
      {
        // handle edge case left-most sensor
        closestDistN = sensorDistances[1];
        closestIndN = 1;
      }
      else if (i == NUM_SENSORS - 1 && sensorDistances[NUM_SENSORS - 2] >= MIN_INTERSECT_DIST)
      {
        // handle edge case right-most sensor
        closestDistN = sensorDistances[NUM_SENSORS - 2];
        closestIndN = NUM_SENSORS - 2;
      }
      else
      {
        // otherwise, if there is a valid neighbor, save the info
        float leftDist = -1;
        float rightDist = -1;

        if (i > 0)
        {
          // get dist of left of curr if exists
          leftDist = sensorDistances[i - 1];
        }

        if (i < NUM_SENSORS - 1)
        {
          // get dist of right of curr if exists
          rightDist = sensorDistances[i + 1];
        }

        if ((rightDist == -1 && leftDist != -1) || leftDist - closestDist < rightDist - closestDist)
        {
          // if invalid right neighbor and valid left neighbor
          // or if target closer to left than right neighbor
          closestDistN = leftDist;
          closestIndN = i - 1;
        }
        else if ((rightDist != -1 && leftDist == -1) || leftDist - closestDist > rightDist - closestDist)
        {
          // if valid right neighbor and invalid left neighbor
          // or if target closer to right than left neighbor
          closestDistN = rightDist;
          closestIndN = i + 1;
        }
        // else do nothing
      }
    }

    // if closest neighbor too far, ignore it
    if (abs(closestDistN - closestDist) > NEIGHBOR_MAX_DIST) {
      closestDistN = MAX_DIST;
      closestIndN = -1;
    }
  }
  // update distP for sensor readings
  distP = (distP + 1) % NUM_DIST;
}

float readSensor(int sensorInd)
{
  float tmp;

  // tmp = distance
  tmp = sensors[sensorInd].ping_cm();

#ifdef RESET_IDLE
  if (tmp > 0) {
    sensorDistancesRaw[sensorInd * NUM_DIST + distP] = tmp;
  }
  else {
    sensorDistancesRaw[sensorInd * NUM_DIST + distP] = MAX_DIST;
  }
#endif

  if (tmp > 0) {
    sensorDistancesRaw[sensorInd * NUM_DIST + distP] = tmp;
  }

  // calculate smoothed
  // tmp = smoothed

  // running median
  float tmpDist[NUM_DIST] = {0};
  for (int i = 0; i < NUM_DIST; i++) {
    tmpDist[i] = sensorDistancesRaw[sensorInd * NUM_DIST + i];
  }
  tmp = QuickMedian<float>::GetMedian(tmpDist, NUM_DIST);

  // save smoothed
  sensorDistances[sensorInd] = tmp;

  // return smoothed
  return tmp;
}

void trigger(int trigPin)
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
}

//#############################################
//#############################################
//#############################################
//####                                    #####
//####              Math                  #####
//####                                    #####
//#############################################
//#############################################
//#############################################

void calculateTargetCoordinates()
{
  // if no valid, reset and face forward
  if (-1 == closestInd)
  {
#ifdef RESET_IDLE
    targetX = 0;
    targetY = 0;
#endif
    return;
  }

  float x, y;

  // intersection of two circles https://math.stackexchange.com/a/256123
  if (closestIndN == -1 || MAX_DIST == closestDistN)
  {
    // no valid neighbor (too close or is edge sensor w/ no overlap)

    if (closestDist > MIN_INTERSECT_DIST && closestIndN == -1 && (closestInd == 0 || closestInd == NUM_SENSORS - 1))
    {
      // go along midline between intersection and edge
      // intersection x^2 + y^2 = r^2
      // y = m(x - c) no need for y intersect
      // m = slope of 15 degree line
      // a = position of sensor closest to target
      // b = position of next sensor closest to target
      // c = x-intercept relative to a
      float a, b, c, m, r;
      m = 3.732f;                          //(Mathf.Sqrt(3) + 1) / (Mathf.Sqrt(3) - 1);
      a = sensorPositions[closestInd * 2]; // x coord of closest
      if (closestInd == 0) {
        b = sensorPositions[2]; // x coord of closest neighbor
      }
      else {
        b = sensorPositions[NUM_SENSORS * 2 - 4]; // x coord of closest neighbor
      }
      r = closestDist;

      // set the x intercept based on if left or right most sensor
      if (closestInd == 0)
      {
        c = (b - a) / 2;
        m = -1 * m; // negative slope
      }
      else
      {
        c = -1 * ((a - b) / 2);
      }

      /**
        Solve for x and plug into y
        y = m(x-c)
        x^2 + y^2 = r^2
      */
      x = (c * m * m + sqrt(-1 * c * c * m * m + r * r + m * m * r * r)) / (1 + m * m);
      y = m * (x - c);

      if (y < 0)
      {
        x = (c * m * m - sqrt(-1 * c * c * m * m + r * r + m * m * r * r)) / (1 + m * m);
        y = m * (x - c);
      }

      if (closestInd != 0)
      {
        x += a;
      }
    }
    else
    {
      x = sensorPositions[closestInd * 2]; // x coord closest
      y = closestDist;
    }
  }
  else
  {
    /**
        (x – x1)^2 + (y – y1)^2 = r1^2
      - (x – x2)^2 + (y – y2)^2 = r2^2

        x^2 – 2xx1 + x1^2 + y^2 – 2yy1 + y1^2 = r1^2
      - x^2 – 2xx2 + x2^2 + y^2 – 2yy2 + y2^2 = r2^2

      2x(x2 – x1) + x1^2 – x2^2 = r1^2 – r2^2

      2x(x2 – x1) = r1^2 – r2^2 – x1^2 + x2^2

      x = (r1^2 – r2^2 – x1^2 + x2^2) / 2(x2 - x1)
      x1 = current sensor
      x2 = neighbor sensor
      r1 = dstance x1 to target
      r2 = distance x2 to target

      r1^2 = (x-x1)^2 + z^2
      z = sqrt(r1^2-(x-x1)^2)
    */
    float r1, r2, x1, x2;
    r1 = closestDist;
    r2 = closestDistN;
    x1 = sensorPositions[closestInd * 2];  // x coord closest
    x2 = sensorPositions[closestIndN * 2]; // x coord closest neighbor
    x = (r1 * r1 - r2 * r2 - x1 * x1 + x2 * x2) / 2 / (x2 - x1);
    y = sqrt(r1 * r1 - (targetX - x1) * (targetX - x1));
  }

  //  Serial.print("x:");
  //  Serial.print(x);
  //  Serial.print("\ty:");
  //  Serial.println(y);

  if (x == x) {
    // testing if x == nan
    targetX = x + X_OFFSET;
  }
  else {
    targetX = 0;
  }

  if (y == y) {
    // test if y == nan
    targetY = constrain(y + Y_OFFSET, 0, MAX_DIST + Y_OFFSET);
  }
  else {
    targetY = 0;
  }
}

//#############################################
//#############################################
//#############################################
//####                                    #####
//####             MOTORS                 #####
//####                                    #####
//#############################################
//#############################################
//#############################################

void updateMotors()
{
  //  Serial.print("\ttargetX: ");
  //  Serial.print(targetX);
  //  Serial.print("\ttargetY: ");
  //  Serial.print(targetY);
  for (int i = 0; i < NUM_MOTORS; i++)
  {
    float targetAngle = 90;

#ifdef RESET_IDLE
    if (targetX == 0 && targetY == 0)
    {
      // reset motor, no target found
      motorAngles[i] = targetAngle;
      continue;
    }
#endif

    // mx/my are motor coords
    // x/y are tmp coords
    float x, y, mx, my;
    mx = motorPositions[i * 2];
    my = motorPositions[i * 2 + 1];
    x = targetX - mx;
    y = targetY - my;

    float ang = atan2(y, x) * 57.296; // 57.296 is 180/pi (rad to deg)

    // adjust for degree
    if (ang < 0)
    {
      ang *= -1;
      ang += 90;
    }

    float diff = ang - motorAngles[i];

    // for SG90, left is 180, right is 0
    if (diff > MOTOR_THRESHOLD)
    {
      // want to decrease angle (move right)
      targetAngle = motorAngles[i] + MOTOR_DELTA + random(DELTA_RANDOM_RANGE + 1);
    }
    else if (diff < -MOTOR_THRESHOLD)
    {
      // want to increase angle (move left)
      targetAngle = motorAngles[i] - MOTOR_DELTA - random(DELTA_RANDOM_RANGE + 1);
    }
    else {
      targetAngle = motorAngles[i];
    }
    targetAngle = constrain(targetAngle, 0, 180);
    motorAngles[i] = targetAngle;
  }
}

void moveMotors()
{
  for (int i = 0; i < NUM_MOTORS; i++)
  {
    motors[i].write(motorAngles[i]);
  }
}
