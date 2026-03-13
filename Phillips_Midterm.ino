/*
  Pin assignments for the bag sensor, indicator lights, buzzer,
  tilt sensor, and the button used to arm or disarm the alarm.
*/
const int bagPin = A2;
const int leftLight = A3;
const int rightLight = 11;

const int buzzerPin = A8;
const int TILT_PIN = 6;
const int alarmButtonPin = 10;

/*
  Motion detection timing and sensitivity settings.

  WINDOW_MS:
  How long to observe tilt sensor changes before deciding whether
  enough motion happened to trigger the alarm.

  CHANGE_THRESHOLD:
  How many valid tilt-state changes must occur within the time window
  to count as motion significant enough to trigger the alarm.

  DEBOUNCE_MS:
  Minimum time required between tilt sensor state changes so that
  noise or rapid chatter is ignored.

  BUTTON_DEBOUNCE_MS:
  Minimum time required for the alarm button reading to remain stable
  before treating it as a real button press.
*/
const unsigned long WINDOW_MS = 2000;
const int CHANGE_THRESHOLD = 8;
const unsigned long DEBOUNCE_MS = 20;
const unsigned long BUTTON_DEBOUNCE_MS = 200;

/*
  Thresholds used to determine whether the bag is considered closed
  or open based on the analog reading from the bag sensor.

  CLOSED_TH:
  Values below this mean the bag is treated as closed.

  OPEN_TH:
  Values above this mean the bag is treated as open.
*/
const int CLOSED_TH = 200;
const int OPEN_TH = 400;

/*
  Frequency used for the alarm buzzer once motion has triggered
  the alarm and it becomes latched.
*/
const int buzzerFreq = 2000;

/*
  State flags for the bag and alarm system.

  isClosed:
  Tracks whether the bag is currently considered closed.

  alarmArmed:
  Tracks whether the movement alarm system is active.

  alarmLatched:
  Tracks whether motion has already triggered the alarm.
  Once true, the buzzer continues until the user disarms the system.
*/
bool isClosed = false;
bool alarmArmed = false;
bool alarmLatched = false;

/*
  Variables used for motion tracking with the tilt sensor.

  lastStableState:
  Stores the most recently accepted stable tilt sensor state.

  lastChangeTime:
  Stores when the last accepted tilt-state change happened.

  windowStart:
  Stores the starting time of the current motion-counting window.

  changeCount:
  Counts how many valid tilt-state changes have happened
  during the current window.
*/
int lastStableState = HIGH;
unsigned long lastChangeTime = 0;

unsigned long windowStart = 0;
int changeCount = 0;

/*
  Variables used to debounce the arm/disarm button.

  lastButtonReading:
  Most recent raw reading from the button pin.

  lastButtonStableState:
  Most recently accepted stable button state after debounce.

  lastButtonChangeTime:
  Time when the raw button reading last changed.
*/
int lastButtonReading = HIGH;
int lastButtonStableState = HIGH;
unsigned long lastButtonChangeTime = 0;

/*
  Plays a short two-tone sound to indicate that the alarm
  has just been armed.
*/
void playArmSound() {
  tone(buzzerPin, 1800, 120);
  delay(150);
  tone(buzzerPin, 2400, 140);
  delay(170);
  noTone(buzzerPin);
}

/*
  Plays a short two-tone sound to indicate that the alarm
  has just been disarmed.
*/
void playDisarmSound() {
  tone(buzzerPin, 2400, 120);
  delay(150);
  tone(buzzerPin, 1800, 140);
  delay(170);
  noTone(buzzerPin);
}

/*
  Resets all motion-tracking values so the system starts fresh
  when the alarm is armed, disarmed, or initialized.
*/
void resetMotionTracking() {
  lastStableState = digitalRead(TILT_PIN);
  lastChangeTime = millis();
  windowStart = millis();
  changeCount = 0;
}

/*
  Arms the alarm system.

  This enables motion monitoring, clears any previously latched
  alarm state, resets the motion-tracking window, and plays the arm
  confirmation sound.
*/
void armAlarm() {
  alarmArmed = true;
  alarmLatched = false;
  resetMotionTracking();
  playArmSound();
  Serial.println("Alarm armed");
}

/*
  Disarms the alarm system.

  This disables motion monitoring, clears any active latched alarm,
  stops the buzzer, resets motion tracking, and plays the disarm
  confirmation sound.
*/
void disarmAlarm() {
  alarmArmed = false;
  alarmLatched = false;
  noTone(buzzerPin);
  resetMotionTracking();
  playDisarmSound();
  Serial.println("Alarm disarmed");
}

/*
  Reads and debounces the arm/disarm button.

  The button uses INPUT_PULLUP, so:
  HIGH means not pressed
  LOW means pressed

  When a stable press is detected, the function toggles the alarm:
  if armed, it disarms it
  if disarmed, it arms it
*/
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

/*
  Runs once when the board powers on or resets.

  Sets up the input and output pin modes, starts Serial
  communication for debugging, and initializes motion tracking.
*/
void setup() {
  pinMode(bagPin, INPUT_PULLUP);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(alarmButtonPin, INPUT_PULLUP);

  pinMode(leftLight, OUTPUT);
  pinMode(rightLight, OUTPUT);

  Serial.begin(9600);

  resetMotionTracking();
}

/*
  Main program loop.

  Responsibilities:
  1. Check whether the alarm button was pressed.
  2. Read the bag sensor and update the open/closed light state.
  3. If the alarm is armed, monitor the tilt sensor for motion.
  4. If enough motion happens within the configured time window,
     latch the alarm and sound the buzzer until disarmed.
*/
void loop() {
  handleAlarmButton();

  /*
    Read the analog bag sensor and decide whether the bag has
    transitioned between closed and open states.

    When the bag closes, both lights are turned off.
    When the bag opens, both lights are turned on.
  */
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

  /*
    If the alarm is not armed, briefly pause, then skip all movement detection logic.
  */
  if (!alarmArmed) {
    delay(20);
    return;
  }

  /*
    If motion has already triggered the alarm, continuously sound
    the buzzer until the user disarms the system.
  */
  if (alarmLatched) {
    tone(buzzerPin, buzzerFreq);
    delay(20);
    return;
  }

  /*
    Read the tilt sensor and count valid state changes.

    A change is only accepted if:
    1. The reading is different from the last stable state
    2. Enough time has passed since the previous accepted change
       to satisfy debounce timing
  */
  unsigned long now = millis();
  int rawState = digitalRead(TILT_PIN);

  if (rawState != lastStableState && (now - lastChangeTime) >= DEBOUNCE_MS) {
    lastStableState = rawState;
    lastChangeTime = now;
    changeCount++;
  }

  /*
    Once the current motion window expires, determine how much
    motion occurred during that window.

    If the number of valid tilt changes reaches the threshold,
    the alarm is latched and the buzzer will start sounding.

    Otherwise, a new window begins and the motion count resets.
  */
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