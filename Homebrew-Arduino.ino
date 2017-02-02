#include <ResponsiveAnalogRead.h>

const int NUM_SAMPLES = 12;
const long LOOP_DELAY = 10000L; // Time between loops in milliseconds
const long SEND_DELAY = 60000L; // Time between every time a temperature is logged to website

const byte HEAT_RELAY_PIN = 8;  // Wire this to the relay module
const byte SENSOR_PIN = A0;

const byte RUNNING = 0;
const byte WAITING = 1;

ResponsiveAnalogRead analog(SENSOR_PIN, false, 0.01);

int currentBatch = 0;
int currentStatus = WAITING;

float setPoint;           // Temperature set point
boolean heating = false;  // Heating status

// Keep track of hours passed
const unsigned long ONE_MINUTE = 60000L;  // One minute in milliseconds
unsigned int hoursPassed = 0;
unsigned int minutesPassed = 0;
unsigned long millisPassed = 0;
unsigned long lastTime = 0;

// Setcurve
String result;  // String from Pi
int points = 0;
int hours[12];
float temps[12];


// ---------------------------------------- setup ----------------------------------------
void setup() {

  analogReference(EXTERNAL);

  pinMode(HEAT_RELAY_PIN, OUTPUT);

  Serial.begin(9600);
}

// ---------------------------------------- insertionSort ----------------------------------------
void insertionSort(int a[], int n) {
  for (int i = 0; i < n; i++) {
    int x = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > x) {
      a[j + 1] = a[j];
      j -= 1;
    }
    a[j + 1] = x;
  }
}

// ---------------------------------------- readSensorValue ----------------------------------------
int readSensorValue() {
  /*
      Take 12 samples with 100 ms in between.
      Remove the 3 smallest and largest sensor readings.
      Then average the remaining values.
  */
  int sensorValues[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    analog.update();
    sensorValues[i] = analog.getValue();
    delay(100);
  }
  insertionSort(sensorValues, NUM_SAMPLES);
  float sensorValue = 0;
  for (int i = 3; i < NUM_SAMPLES - 3; i++) {
    sensorValue += sensorValues[i];
  }
  sensorValue /= (NUM_SAMPLES - 6);

  return sensorValue;
}

// ---------------------------------------- controlHeat ----------------------------------------
int lowTempCounter = 0;
int highTempCounter = 0;
void controlHeat(float temp) {

  if (points > 0) { // If there is a set curve

    // Find out how far we've come in the process
    int j = 0;
    while (j < points && hoursPassed >= hours[j]) {
      j++;
    }

    if (j < points) { // If not finished with the process

      // Interpolate value between two points
      float derivative = (temps[j] - temps[j - 1]) / ((float)hours[j] - (float)hours[j - 1]);
      setPoint = temps[j - 1] + (hoursPassed + minutesPassed / 60.0  - hours[j - 1]) * derivative;

      if (temp < (setPoint - 0.05) && !heating) {
        lowTempCounter++;
        highTempCounter = 0;
        if (lowTempCounter > 2) { // 3 values below set point required to start heating
          digitalWrite(HEAT_RELAY_PIN, HIGH);
          heating = true;
        }
      } else if (temp >= (setPoint + 0.1) && heating) {
        highTempCounter++;
        lowTempCounter = 0;
        if (highTempCounter > 2) {
          digitalWrite(HEAT_RELAY_PIN, LOW);
          heating = false;
        }
      } else {
        lowTempCounter = 0;
        highTempCounter = 0;
      }

    } else {  // We are finished
      currentStatus = WAITING;
      digitalWrite(HEAT_RELAY_PIN, LOW);  // Turn off heating
      heating = false;
    }
  } else {  // No set curve
    currentStatus = WAITING;
    digitalWrite(HEAT_RELAY_PIN, LOW);  // Turn off heating
    heating = false;
  }
}

// ---------------------------------------- updateTime ----------------------------------------
void updateTime() {
  /*
      Keep track of hours and minutes passed
  */
  if (currentStatus == RUNNING) {
    unsigned long currentTime = millis();
    if (currentTime < lastTime) { // Handle overflow
      lastTime = currentTime;
    }
    millisPassed += currentTime - lastTime;
    lastTime = currentTime;
    if (millisPassed > ONE_MINUTE) {
      minutesPassed++;
      millisPassed = 0;
    }
    if (minutesPassed > 60) {
      hoursPassed++;
      minutesPassed = 0;
    }
  } else {  // Not running
    // Reset everything
    millisPassed = 0;
    minutesPassed = 0;
    hoursPassed = 0;
  }
}

// ---------------------------------------- parseResult ----------------------------------------
void parseResult(String result) {
  /*
      Parse the resultstring recieved from the Pi
  */
  int index = result.indexOf(';');
  int batchNum = result.substring(0, index).toInt();

  if (batchNum < 0) { // Abort batch
    millisPassed = 0;
    minutesPassed = 0;
    hoursPassed = 0;
    points = 0;
    currentStatus = WAITING;
    currentBatch--;
    return;
  } else if (batchNum <= currentBatch) { // Not a new batch
    return;
  }

  // We have a new batch
  currentBatch = batchNum;
  currentStatus = RUNNING;
  result = result.substring(index + 1);
  millisPassed = 0;
  minutesPassed = 0;
  hoursPassed = 0;

  int i = 0;
  while (true) {
    int index1 = result.indexOf(',');
    int index2 = result.indexOf(';');

    if (index1 >= 0) {
      int hour = result.substring(0, index1).toInt();
      float temp = result.substring(index1 + 1, index2).toFloat();

      hours[i] = hour;
      temps[i] = temp;
      i++;
    } else {
      break;
    }

    result = result.substring(index2 + 1);
  }
  points = i;
}

// ---------------------------------------- loop ----------------------------------------
unsigned long lastTimeMillis = 0;
void loop() {

  // Handle overflow
  if (millis() < lastTimeMillis) {
    lastTimeMillis = millis();
  }

  updateTime();

  float sensorValue = readSensorValue();

  // Calculate temperature based on formula. Formula found by testing.
  float tempC = 0.000075457612181 * sensorValue * sensorValue + 0.034084433378039 * sensorValue - 13.556216402377600;

  if (currentStatus == RUNNING && millis() - lastTimeMillis > SEND_DELAY) {
    lastTimeMillis = millis();
    
    // Send data to website
    Serial.println("temp=" + String(tempC, 1));
  }

  // Control the heating relay
  controlHeat(tempC);

  // Get setpoints from website
  String settings = Serial.readString();
  if (settings.charAt(0) == ';' && settings.charAt(0) == ';')
  {
    parseResult(settings.substring(2));
  }

  delay(LOOP_DELAY);

}
