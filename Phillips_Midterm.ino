// Pins
const int bagPin = A2;
const int leftLight = A3;
const int rightLight = 11;

const int buzzerPin = A8;
const int TILT_PIN = 6;
const int alarmButtonPin = 10;

const unsigned long WINDOW_MS = 2000;  
const int CHANGE_THRESHOLD = 8;      
const unsigned long DEBOUNCE_MS = 20;   
const unsigned long BUTTON_DEBOUNCE_MS = 200;

// below this = closed
const int CLOSED_TH = 200;

// above this = open
const int OPEN_TH = 400;

const int buzzerFreq = 2000;

bool isClosed = false;
bool alarmArmed = false;
bool alarmLatched = false;

int lastStableState = HIGH;          
unsigned long lastChangeTime = 0;

unsigned long windowStart = 0;
int changeCount = 0;

// Button tracking
int lastButtonReading = HIGH;
int lastButtonStableState = HIGH;
unsigned long lastButtonChangeTime = 0;

void playArmSound() {
  tone(buzzerPin, 1800, 120);
  delay(150);
  tone(buzzerPin, 2400, 140);
  delay(170);
  noTone(buzzerPin);
}

void playDisarmSound() {
  tone(buzzerPin, 2400, 120);
  delay(150);
  tone(buzzerPin, 1800, 140);
  delay(170);
  noTone(buzzerPin);
}

void resetMotionTracking() {
  lastStableState = digitalRead(TILT_PIN);
  lastChangeTime = millis();
  windowStart = millis();
  changeCount = 0;
}

void armAlarm() {
  alarmArmed = true;
  alarmLatched = false;
  resetMotionTracking();
  playArmSound();
  Serial.println("Alarm armed");
}

void disarmAlarm() {
  alarmArmed = false;
  alarmLatched = false;
  noTone(buzzerPin);
  resetMotionTracking();
  playDisarmSound();
  Serial.println("Alarm disarmed");
}

void handleAlarmButton() {
  int reading = digitalRead(alarmButtonPin);
  unsigned long now = millis();

  if (reading != lastButtonReading) {
    lastButtonChangeTime = now;
    lastButtonReading = reading;
  }

  if ((now - lastButtonChangeTime) >= BUTTON_DEBOUNCE_MS) {
    if (reading != lastButtonStableState) {
      lastButtonStableState = reading;

      // Button press detected
      if (lastButtonStableState == LOW) {
        if (alarmArmed) {
          disarmAlarm();
        } else {
          armAlarm();
        }
      }
    }
  }
}

void setup() {
  pinMode(bagPin, INPUT_PULLUP); 
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(alarmButtonPin, INPUT_PULLUP);

  pinMode(leftLight, OUTPUT);
  pinMode(rightLight, OUTPUT);

  Serial.begin(9600);

  resetMotionTracking();
}

void loop() {
  handleAlarmButton();

  // Bag open/close logic
  int v = analogRead(bagPin);

  if (!isClosed && v < CLOSED_TH) {
    isClosed = true;
    digitalWrite(leftLight, LOW);
    digitalWrite(rightLight, LOW);
    Serial.print("Bag Closed, analog=");
    Serial.println(v);
  } else if (isClosed && v > OPEN_TH) {
    isClosed = false;
    digitalWrite(leftLight, HIGH);
    digitalWrite(rightLight, HIGH);
    Serial.print("Bag Open, analog=");
    Serial.println(v);
  }

  // Only do alarm movement logic when armed
  if (!alarmArmed) {
    delay(20);
    return;
  }

  // If alarm has been triggered, keep buzzer sounding until disarmed
  if (alarmLatched) {
    tone(buzzerPin, buzzerFreq);
    delay(20);
    return;
  }

  unsigned long now = millis();
  int rawState = digitalRead(TILT_PIN);

  if (rawState != lastStableState && (now - lastChangeTime) >= DEBOUNCE_MS) {
    lastStableState = rawState;
    lastChangeTime = now;
    changeCount++;
  }

  if ((now - windowStart) >= WINDOW_MS) {
    if (changeCount >= CHANGE_THRESHOLD) {
      alarmLatched = true;
      Serial.println("ALARM TRIGGERED: motion detected");
    } else {
      changeCount = 0;
      windowStart = now;
    }
  }

  delay(20);
}