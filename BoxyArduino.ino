#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_UART.h"

//// Define Variables
// Modes
#define DEVELOPMENT_MODE               1
#define FACTORYRESET_ENABLE            1
#define MINIMUM_FIRMWARE_VERSION       "0.6.6"
#define MODE_LED_BEHAVIOUR             "MODE"

// Bluetooth
#define BUFSIZE                        128        // Size of the read buffer for incoming data
#define VERBOSE_MODE                   true       // If set to 'true' enables debug output
#define BLE_READPACKET_TIMEOUT         500        // Timeout in ms waiting to read a response
#define BLUEFRUIT_HWSERIAL_NAME        Serial1
#define BLUEFRUIT_UART_MODE_PIN        -1

// Pins
#define OLED_RESET 8
#define UP_BUTTON 9
#define DOWN_BUTTON 10
#define MODE_BUTTON 11
#define LED_PIN 12

// Button State
static int up_button_state = 0;
static int down_button_state = 0;
static int mode_button_state = 0;

// Constaints
const String DASH = "-";
const int MAX_WORKOUTS = 4;
const char WCHAR = 'W';
const char CCHAR = 'C';
const char DCHAR = 'D';
const char FCHAR = 'F';
const char TCHAR = 'T';
const char SCHAR = 'S';
const char RCHAR = 'R';
const char DASHCHAR = '-';
const int REST_TIME = 5000;
const int REP_THRESHOLD = 1;
const int BALANCE_THRESHOLD = 12;

// Local Workout Storage
static String workoutNames[MAX_WORKOUTS] = {""};
static int workoutWeights[MAX_WORKOUTS] = {0};
static int workoutSets[MAX_WORKOUTS] = {0};
static int workoutReps[MAX_WORKOUTS] = {0};
static int workoutNamesIndex = 0;
static int workoutWeightsIndex = 0;
static int workoutSetsIndex = 0;
static int workoutRepsIndex = 0;
static int workoutLength = 0;
static int ack = 0;
static int workoutWeightsCompleted[MAX_WORKOUTS] = {0};
static int workoutSetsCompleted[MAX_WORKOUTS] = {0};
static int workoutRepsCompleted[MAX_WORKOUTS] = {0};
static int currentWorkoutIndex = 0;

// Initialize OLED
Adafruit_SSD1306 display(OLED_RESET);

// Initialize Accelerometer
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(51423);

// Initialize BLE
Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

void setup(void) {
  // Display Setup
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.println("Boxy");
  display.setCursor(5, 5);
  display.display();
  
  if (DEVELOPMENT_MODE) {
    while (!Serial);
    delay(500);
  }

  Serial.begin(115200);
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if (!ble.begin(VERBOSE_MODE)) {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if (FACTORYRESET_ENABLE) {
    Serial.println(F("Performing a factory reset: "));
    if (!ble.factoryReset()){
      error(F("Couldn't factory reset"));
    }
  }

  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  ble.info();
  ble.verbose(false);

  Serial.println(F("**************************************"));

  // Change BLE Name
  Serial.println(F("Change BLE Device Name to: Boxy"));
  if (!ble.sendCommandCheckOK(F("AT+GAPDEVNAME=Boxy"))) {
    error(F("Could not set device name?"));
  }

  if (ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION)) {
    // Change LED mode
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set Bluefruit to DATA mode
  Serial.println( F("Setting bluetooth module to DATA mode"));
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("**************************************"));

  // Accelerometer Setup
  accel.begin();

  // Button Setup
  pinMode(UP_BUTTON, INPUT);
  pinMode(DOWN_BUTTON, INPUT);
  pinMode(MODE_BUTTON, INPUT);

  // LED Setup
  pinMode(LED_PIN, OUTPUT);
}

void loop(void) {
  // Wait for BLE connection
  if (ble.isConnected()) {
    bool receivedPacket = false;
    String tmp = "";
    char type = 'X';
    char *packetBuffer = "";
    
    // Check for incoming characters from Bluefruit
    ble.println("AT+BLEUARTRX");
    ble.readline();
    if (strcmp(ble.buffer, "OK") != 0) {
      packetBuffer = ble.buffer;
      Serial.print(F("[Recv] "));
      Serial.println(packetBuffer);
      receivedPacket = true;
    }

    if (receivedPacket) {
      if (packetBuffer[0] == WCHAR || packetBuffer[0] == CCHAR) {
        if (packetBuffer[0] == WCHAR) {
          workoutLength = packetBuffer[1] - '0';
        }
    
        if (packetBuffer[3] == TCHAR) {
          type = TCHAR;
        } else if (packetBuffer[3] == WCHAR) {
          type = WCHAR;
        } else if (packetBuffer[3] == SCHAR) {
          type = SCHAR;
        } else if (packetBuffer[3] == RCHAR) {
          type = RCHAR;
        }

        int index = 5;
        String bufferString = packetBuffer;
        int bufferLength = bufferString.length();
        while (index < bufferLength) {
          if (packetBuffer[index] == DASHCHAR) {
            if (type == TCHAR) {
              workoutNames[workoutNamesIndex] = tmp;
              workoutNamesIndex++;
            } else if (type == WCHAR) {
              workoutWeights[workoutWeightsIndex] = tmp.toInt();
              workoutWeightsCompleted[workoutWeightsIndex] = tmp.toInt();
              workoutWeightsIndex++;
            } else if (type == SCHAR) {
              workoutSets[workoutSetsIndex] = tmp.toInt();
              workoutSetsIndex++;
            } else if (type == RCHAR) {
              workoutReps[workoutRepsIndex] = tmp.toInt();
              workoutRepsIndex++;
            }
      
            tmp = "";
          } else {
            tmp += String(packetBuffer[index]);
          }
          index++;
        }

        ble.print("AT+BLEUARTTX=");
        ble.print("A");
        ble.print(ack);
        if (!ble.waitForOK()) {
          Serial.println(F("Failed to send"));
        }
        
        ack++;
      } else if (packetBuffer[0] == FCHAR) {
        workoutNamesIndex = 0;
        workoutWeightsIndex = 0;
        workoutSetsIndex = 0;
        workoutRepsIndex = 0;
        ack = 0;

        int l = 0;
        while (l < MAX_WORKOUTS) {
          if (workoutNames[l] != "") {
            l++;
          } else {
            break;
          }
        }

        for (int i = 0; i < l; i += 1) {
          Serial.print(workoutNames[i]);
          Serial.print("-");
          Serial.print(workoutWeights[i]);
          Serial.print("-");
          Serial.print(workoutWeightsCompleted[i]);
          Serial.print("-");
          Serial.print(workoutReps[i]);
          Serial.print("-");
          Serial.print(workoutSets[i]);
          Serial.println();
        }
      } else if (packetBuffer[0] == DCHAR) {
        int l = 0;
        while (l < MAX_WORKOUTS) {
          if (workoutNames[l] != "") {
            l++;
          } else {
            break;
          }
        }

        for (int i = 0; i < l; i++) {
          String res = "";
          if (i == 0) {
            res = "S";
            res += String(l);
            res += DASH;
          } else {
            res = "C";
            res += String(i + 1);
            res += DASH;
          }

          // Workout Name
          res += workoutNames[i].charAt(0);
          res += DASH;

          // Weight
          res += String(workoutWeightsCompleted[i]);
          res += DASH;

          // Sets
          res += String(workoutSetsCompleted[i]);
          res += DASH;

          // Reps
          res += String(workoutRepsCompleted[i]);
          res += DASH;

          Serial.println("----------Response---------");
          Serial.println(res);
          Serial.println("---------------------------");

          ble.print("AT+BLEUARTTX=");
          ble.println(res);
          if (!ble.waitForOK()) {
            Serial.println(F("Failed to send"));
          }
          delay(100);
        }
      }
    }
    
    receivedPacket = false;
  }

  // Detect Button Press
//  up_button_state = digitalRead(UP_BUTTON);
//  down_button_state = digitalRead(DOWN_BUTTON);
//  mode_button_state = digitalRead(MODE_BUTTON);
//  if (up_button_state == HIGH) {
//    workoutWeightsCompleted[currentWorkoutIndex] += 5;
//  } else if (down_button_state == HIGH) {
//    workoutWeightsCompleted[currentWorkoutIndex] -= 5;
//  } else if (mode_button_state == HIGH) {
//    currentWorkoutIndex += 1;
//    currentWorkoutIndex = currentWorkoutIndex % MAX_WORKOUTS;
//  }

  // Detect and Handle Reps
  if (repDetected()) {
    workoutRepsCompleted[currentWorkoutIndex] += 1;
    
    if (workoutRepsCompleted[currentWorkoutIndex] == workoutReps[currentWorkoutIndex]) {
      workoutSetsCompleted[currentWorkoutIndex] += 1;
      workoutRepsCompleted[currentWorkoutIndex] = 0;
      printScreen(workoutStringAtIndex(currentWorkoutIndex));
      delay(REST_TIME);
      restOver();
    }
  }

  // Detect and Handle Balance
  if (offBalance()) {
    printScreen("\n Off\n Balance!");
  }
}

//// Additional Functions
String workoutStringAtIndex(int index) {
  String workoutString = "";
  workoutString += workoutNames[index];

  int i = 0;
  while (i < workoutSets[index]) {
    workoutString += " (";
    workoutString += String(workoutReps[index]);
    workoutString += ")";
  }

  Serial.println(workoutString);
//  return workoutString;
  return "hello";
}

void printScreen(String msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println(msg);
  display.display();
}

bool repDetected() {
  sensors_event_t event;
  accel.getEvent(&event);
  
  if (event.acceleration.y > REP_THRESHOLD) {
    return 1;
  } else {
    return 0;   
  }
}

bool offBalance() {
  sensors_event_t event;
  accel.getEvent(&event);
  
  if (pow(event.acceleration.x, 2) + pow(event.acceleration.y, 2) > BALANCE_THRESHOLD) {
    return 1;
  } else {
    return 0;
  }
}

void restOver() {
  printScreen("\n Rest \n Over!");
  digitalWrite(LED_PIN, HIGH);
}

// Error Helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

