#include <ResponsiveAnalogRead.h>

const int NUM_SAMPLES = 12;
const long LOOP_DELAY = 10000L; // Time between loops in milliseconds
const long SEND_DELAY = 60000L; // Time between updates sent to website

const byte HEAT_RELAY_PIN = 8;  // Wire this to the relay module
const byte SENSOR_PIN = A0;

const byte RUNNING = 0;
const byte WAITING = -1;

ResponsiveAnalogRead analog(SENSOR_PIN, false, 0.1);

int currentStatus = WAITING;

float setPoint;           // Temperature set point
boolean heating = false;  // Heating status

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

  if (currentStatus == RUNNING) {

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

  } else {
    digitalWrite(HEAT_RELAY_PIN, LOW);  // Turn off heating
    heating = false;
  }
}

// ---------------------------------------- parseResult ----------------------------------------
void parseResult(String result) {
  /*
      Parse the resultstring recieved from the Pi
  */

  if (result.charAt(0) == ';' && result.charAt(1) == ';') {
    result = result.substring(2);

    if (result == "-1.0") {
      currentStatus = WAITING;
    } else {
      currentStatus = RUNNING;
      setPoint = result.toFloat();
    }
  }
}

// ---------------------------------------- loop ----------------------------------------
unsigned long lastSendTime = 0;
void loop() {
  unsigned long currentTime = millis();
  
  // Handle overflow
  if (currentTime < lastSendTime) {
    lastSendTime = 0;
  }

  float sensorValue = readSensorValue();

  // Calculate temperature based on formula. Formula found by testing.
  float tempC = 0.000026534 * sensorValue * sensorValue + 0.078856745 * sensorValue - 23.377122215; //0.000075457612181 * sensorValue * sensorValue + 0.034084433378039 * sensorValue - 13.556216402377600;

  if (currentStatus == RUNNING && currentTime - lastSendTime > SEND_DELAY) {
    lastSendTime = currentTime;
    // Send data to website
    Serial.println("temp=" + String(tempC, 1) + "&heating=" + heating ? String(1) : String(0));
  }

  // Control the heating relay
  controlHeat(tempC);

  // Get setpoints from website
  parseResult(Serial.readString());

  delay(LOOP_DELAY);

}
