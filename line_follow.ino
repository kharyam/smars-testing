#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// OLED Related
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_ADDRESS = 0x3C;


// Motor Related
const int RIGHT_MOTOR_DIRECTION_PIN = 12;
const int RIGHT_MOTOR_SPEED_PIN = 10;
const int LEFT_MOTOR_DIRECTION_PIN = 13;
const int LEFT_MOTOR_SPEED_PIN = 11;
const float RIGHT_MOTOR_SLOWDOWN = 1;

const int BUZZER_PIN = 4;

const String STATE_FORWARD = "FORWARD";
const String STATE_REVERSE = "REVERSE";
const String STATE_STOPPED = "STOPPED";
const String STATE_LEFT = "LEFT";
const String STATE_RIGHT = "RIGHT";
const String TOGGLE_MODE = "TOGGLE_MODE";
const String SPEED_UP = "SPEED_UP";
const String SPEED_DOWN = "SPEED_DOWN";

const String LINE_FOLLOW_START = "====\n0 START";

const int SPEED_STEP = 10;
const int TURN_DELAY_MS = 50;
const float TURN_FACTOR = .8;

// IR
const int RIGHT_IR_PIN = A0;
const int LEFT_IR_PIN = A1;

// Distance Sensor
const int TRIGGER_PIN = 7;
const int ECHO_PIN = 8;


String currentMotorState = STATE_STOPPED;
int startOffset;
bool isLineFollowMode = false;
int currentSpeed = 180;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayInitialized = false;
String bodyText = "";

void setup() {

  pinMode(RIGHT_MOTOR_DIRECTION_PIN, OUTPUT);
  pinMode(LEFT_MOTOR_DIRECTION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(9600);

  if (displayPresent() && display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    displayInitialized = true;
  }
}

bool displayPresent() {
  Wire.begin();
  Wire.beginTransmission(OLED_ADDRESS);
  int error = Wire.endTransmission();
  Wire.end();
  return error==0;
}

void displayText(String text = bodyText) {
  bodyText = text;
  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    int mils = millis();
    int mins = mils / 60000;
    int seconds = (mils / 1000) % 60;
    char uptime[7];
    sprintf(uptime, "%03d:%02d", mins, seconds);
    display.println(String("Mode: ") + (isLineFollowMode ? "Follow" : "Command") + "  Uptime: " + uptime);
    display.setCursor(0, 10);
    display.println(currentMotorState + " Speed: " + currentSpeed + "Voltage: " + 1000.0 * readVcc());
    display.setCursor(0, 20);
    display.setTextSize(1);
    if (bodyText != "") {
      display.println(bodyText);
    }
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
  } else {
    startOffset = millis();
    Serial.println(LINE_FOLLOW_START);
  }
}

void log(String command) {
  if (currentMotorState != command) {
    Serial.print(millis() - startOffset);
    Serial.print(" ");
    Serial.println(command);
    displayText();
  }
  currentMotorState = command;
}

void forward(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed;
  }

  log(STATE_FORWARD);
  digitalWrite(BUZZER_PIN, LOW);
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

  digitalWrite(BUZZER_PIN, HIGH);
}


void left(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed * TURN_FACTOR;
  }

  log(STATE_LEFT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed * RIGHT_MOTOR_SLOWDOWN);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);
  delay(TURN_DELAY_MS);
}

void right(int speed = -1) {

  if (speed == -1) {
    speed = currentSpeed * TURN_FACTOR;
  }

  log(STATE_RIGHT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, LOW);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, speed * RIGHT_MOTOR_SLOWDOWN);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(LEFT_MOTOR_SPEED_PIN, speed);
  delay(TURN_DELAY_MS);
}

void stop() {

  log(STATE_STOPPED);
  digitalWrite(BUZZER_PIN, LOW);

  digitalWrite(RIGHT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(RIGHT_MOTOR_SPEED_PIN, 0);

  digitalWrite(LEFT_MOTOR_DIRECTION_PIN, HIGH);
  analogWrite(LEFT_MOTOR_SPEED_PIN, 0);
}

void speedUp() {
  currentSpeed = min(currentSpeed + SPEED_STEP, 255);
  executeCommand(currentMotorState);
}

void speedDown() {
  currentSpeed = max(0, currentSpeed - SPEED_STEP);
  executeCommand(currentMotorState);
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
  if (detectLineLeft()) {
    left();
  } else if (detectLineRight()) {
    right();
    // } else if (wall_distance_cm()<3) {
    //   stop();
  } else {
    forward();
  }
}

void executeCommand(String command) {
  if (command.equals(STATE_FORWARD)) {
    forward();
  } else if (command.equals(STATE_REVERSE)) {
    reverse();
  } else if (command.equals(STATE_LEFT)) {
    left();
  } else if (command.equals(STATE_RIGHT)) {
    right();
  } else if (command.equals(TOGGLE_MODE)) {
    toggleMode();
  } else if (command.equals(SPEED_UP)) {
    speedUp();
  } else if (command.equals(SPEED_DOWN)) {
    speedDown();
  } else if (command.equals(STATE_STOPPED)) {
    stop();
  }
}

void processCommands() {

  String command = "";
  while (Serial.available()) {
    command += char(Serial.read());
    delay(5);  // Add slight delay to give time for input to come in
  }

  if (!Serial.available() && command != "") {
    executeCommand(command);
  }
}

void loop() {
  processCommands();
  if (isLineFollowMode) {
    lineFollowMode();
  }
}