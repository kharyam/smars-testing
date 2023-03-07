#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <Button.h>

// OLED Related
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define DISPLAY_REDRAW_RATE_MS 500

// Motor Related
#define RIGHT_MOTOR_DIRECTION_PIN 12
#define RIGHT_MOTOR_SPEED_PIN 10
#define LEFT_MOTOR_DIRECTION_PIN 13
#define LEFT_MOTOR_SPEED_PIN 11
#define RIGHT_MOTOR_SLOWDOWN 1
#define DEFAULT_SPEED 180

#define BUZZER_PIN 4
#define BUTTON_PIN 5

const char* STATE_FORWARD = "FORWARD";
const char* STATE_REVERSE = "REVERSE";
const char* STATE_STOPPED = "STOPPED";
const char* STATE_LEFT = "LEFT";
const char* STATE_RIGHT = "RIGHT";

const char* COMMAND_FORWARD = "F";
const char* COMMAND_BACK = "B";
const char* COMMAND_LEFT = "L";
const char* COMMAND_RIGHT = "R";
const char* COMMAND_STOP = "S";
const char* COMMAND_HORN_ON = "V";
const char* COMMAND_HORN_OFF = "v";
const char* COMMAND_SPEED_100 = "q";

const char* COMMAND_LINEFOLLOW_ON = "X";
const char* COMMAND_LINEFOLLOW_OFF = "x";

const char* COMMAND_MODE = "TOGGLE_MODE";
const char* COMMAND_SPEED_UP = "SPEED_UP";
const char* COMMAND_SPEED_DOWN = "SPEED_DOWN";

const char* LINE_FOLLOW_START = "====\n0 START";

#define SPEED_STEP 10
#define TURN_FACTOR .8f

// IR
#define RIGHT_IR_PIN A0
#define LEFT_IR_PIN A1

// // Distance Sensor
#define TRIGGER_PIN 7
#define ECHO_PIN 8

const char* currentMotorState = STATE_STOPPED;
int startOffset;
bool isLineFollowMode = false;
int currentSpeed = DEFAULT_SPEED;

bool displayInitialized = false;
String bodyText = "";

unsigned long lastupdate = 0;
Button button(BUTTON_PIN);

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool displayPresent() {
  Wire.begin();
  Wire.beginTransmission(SCREEN_ADDRESS);
  int error = Wire.endTransmission();
  Wire.end();
  return error == 0;
}

void statusDisplay(bool force = false) {

  unsigned long mils = millis();

  if (displayInitialized && !isLineFollowMode && (force || mils - lastupdate > DISPLAY_REDRAW_RATE_MS)) {
    lastupdate = mils;
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    int mins = mils / 60000L;
    int seconds = (mils / 1000L) % 60L;
    char uptime[50];
    long vcc = readVcc();
    int fraction = (vcc % 1000);
    sprintf(uptime, "%02d:%02d", mins, seconds);
    display.println(uptime);
    display.setTextSize(1);
    display.setCursor(70, 0);
    display.println(String("") + vcc / 1000 + "." + vcc % 1000 + "V");
    display.drawRoundRect(70, 9, 50, 7, 5, WHITE);
    int width = currentSpeed * 50 / 255;
    for (int y = 10; y < 16; y++) {
      display.drawFastHLine(71, y, width, WHITE);
    }
    display.setCursor(0, 22);
    display.setTextSize(2);
    display.println(isLineFollowMode ? "FOLLOW" : "COMMAND");
    display.setCursor(0, 42);
    display.println(currentMotorState);
    display.stopscroll();

    if (detectLineLeft()) {
      for (int y = 0; y < 7; y++)
        display.drawFastHLine(100, y, 10, WHITE);
    }

    if (detectLineRight()) {

      for (int y = 0; y < 7; y++)
        display.drawFastHLine(116, y, 10, WHITE);
    }

    display.display();  // 40ms, 20ms for everything else
  }
}

void staticDisplay(const char* text, const int size = 3) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(WHITE);
  display.setCursor(0, 22);
  display.println(text);
  for (int y = 0; y < 16; y++) {
    display.drawFastHLine(0, y, 128, WHITE);
  }

  display.startscrollright(0x00, 0x0E);
  display.display();
}

void setup() {

  Serial.begin(9600);
  pinMode(RIGHT_MOTOR_DIRECTION_PIN, OUTPUT);
  pinMode(LEFT_MOTOR_DIRECTION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    displayInitialized = true;
    display.display();
  }
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);             // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);  // Convert
  while (bit_is_set(ADCSRA, ADSC))
    ;
  result = ADCL;
  result |= ADCH << 8;
  result = 1126400L / result;  // Back-calculate AVcc in mV
  return result;               //Vcc in mV
}

void toggleMode() {
  isLineFollowMode = !isLineFollowMode;

  if (!isLineFollowMode) {
    stop();
    statusDisplay(true);
  } else {
    currentSpeed = DEFAULT_SPEED;
    staticDisplay("FOLLOW");

    for (int i = 0; i < 5; i++) {
      buzzerOn();
      delay(150);
      buzzerOff();    
      delay(50);
    }
    startOffset = millis();
    Serial.println(LINE_FOLLOW_START);
  }
}

void log(const char* command) {
  if (strcmp(currentMotorState, command) != 0) {
    Serial.print(millis() - startOffset);
    Serial.print(" ");
    Serial.println(command);
    currentMotorState = command;
    statusDisplay(true);
  }
}

void buzzerOn() {
  digitalWrite(BUZZER_PIN, HIGH);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

void forward(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed;
  }

  log(STATE_FORWARD);
  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed * RIGHT_MOTOR_SLOWDOWN);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);
}

void reverse(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed;
  }

  log(STATE_REVERSE);
  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed * RIGHT_MOTOR_SLOWDOWN);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);
}


void left(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed * TURN_FACTOR;
  }

  log(STATE_LEFT);
  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed * RIGHT_MOTOR_SLOWDOWN);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);
}

void right(int speed = -1) {

  log(STATE_RIGHT);
  if (speed == -1) {
    speed = currentSpeed * TURN_FACTOR;
  }

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);

  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed);
}

void stop() {

  log(STATE_STOPPED);

  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, 0);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(LEFT_MOTOR_SPEED_PIN, 0);
}

void setSpeed(int speed) {
  currentSpeed = speed;
  executeCommand(currentMotorState);
}

void speedUp() {
  setSpeed(min(currentSpeed + SPEED_STEP, 255));
}

void speedDown() {
  setSpeed(max(0, currentSpeed - SPEED_STEP));
}

bool irSensorLine(int irPin) {
  int val = analogRead(irPin);
  if (val > 800) {
    return true;
  }
  return false;
}

bool detectLineRight() {
  return irSensorLine(RIGHT_IR_PIN);
}

bool detectLineLeft() {
  return irSensorLine(LEFT_IR_PIN);
}

int wallDistanceCM() {
  // Clears the trigPin
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIGGER_PIN, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  int duration = pulseIn(ECHO_PIN, HIGH);
  // Calculating the distance
  int distance = duration * 0.034 / 2;
  // Prints the distance on the Serial Monitor

  if (distance < 500) {
    return distance;
  }

  return 0;
}

void lineFollowMode() {
  if (detectLineLeft() && strcmp(currentMotorState, COMMAND_RIGHT) != 0) {
    left();
  } else if (detectLineRight() && strcmp(currentMotorState, COMMAND_LEFT) != 0) {
    right();
  } else {
    forward();
  }
}

void executeCommand(const char* command) {

  bool resetMode = true;
  if (se(command, STATE_FORWARD) || se(command, COMMAND_FORWARD)) {
    forward();
  } else if (se(command, STATE_REVERSE) || se(command, COMMAND_BACK)) {
    reverse();
  } else if (se(command, STATE_LEFT) || se(command, COMMAND_LEFT)) {
    left();
  } else if (se(command, STATE_RIGHT) || se(command, COMMAND_RIGHT)) {
    right();
  } else if (se(command, COMMAND_MODE) || se(command, COMMAND_LINEFOLLOW_ON) || se(command, COMMAND_LINEFOLLOW_OFF)) {
    toggleMode();
    resetMode = false;
  } else if (se(command, COMMAND_SPEED_UP)) {
    speedUp();
    resetMode = false;
  } else if (se(command, COMMAND_SPEED_DOWN)) {
    speedDown();
    resetMode = false;
  } else if (se(command, STATE_STOPPED) || se(command, COMMAND_STOP)) {
    stop();
  } else if (se(command, COMMAND_SPEED_100)) {
    setSpeed(255);
  } else if (atoi(command) >= 0 && atoi(command) < 10) {
    setSpeed(25 * atoi(command));
  } else {
    return;
  }

  if (resetMode && isLineFollowMode) {
    toggleMode();
  }
}

bool se(const char* a, const char* b) {
  return strcmp(a, b) == 0;
}

void processCommands() {

  String command = "";
  while (Serial.available()) {
    command += char(Serial.read());
    delay(5);  // Add slight delay to give time for input to come in
  }

  if (!Serial.available() && command != "") {
    executeCommand(command.c_str());
  }
}


void checkButton() {
  if (button.pressed()) {
    toggleMode();
  }
}

void loop() {
  checkButton();
  processCommands();
  if (isLineFollowMode) {
    lineFollowMode();
  }
  statusDisplay();
}