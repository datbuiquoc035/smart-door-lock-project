#include <Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ============================ Pins ============================
const byte PIN_SERVO     = 10;
const byte PIN_LED_GREEN = 11;
const byte PIN_LED_RED   = 12;
const byte PIN_BUZZER    = 13;

// ============================ Keypad ============================
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};  // R1..R4
byte colPins[COLS] = {5, 4, 3};     // C1..C3
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============================ LCD ============================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================ Servo ============================
Servo servo;
const int ANGLE_LOCKED   = 0;
const int ANGLE_UNLOCKED = 90;

// ============================ Cau hinh ============================
const byte   PIN_LEN         = 4;
const byte   MAX_ATTEMPTS    = 3;
const unsigned long LOCKOUT_MS     = 30000UL;   // khoa ban phim 30s sau 3 lan sai
const unsigned long AUTO_RELOCK_MS = 5000UL;    // tu dong khoa lai sau 5s
const unsigned long SERVO_STEP_MS  = 15UL;      // toc do quay servo (mo phong thuc te)
const unsigned long RED_FLASH_MS   = 400UL;
const unsigned long WRONG_MSG_MS   = 1500UL;    // thoi gian hien thong bao sai ma

const int EEPROM_ADDR = 0;
char masterPin[PIN_LEN + 1] = "1234";

// ============================ Trang thai ============================
enum State { S_LOCKED, S_UNLOCKED,
             S_CHANGE_VERIFY, S_CHANGE_NEW, S_CHANGE_CONFIRM,
             S_LOCKOUT };
State state = S_LOCKED;

char entry[PIN_LEN + 1];
byte entryLen = 0;
byte attempts = 0;
char newPin[PIN_LEN + 1];

unsigned long unlockAt      = 0;
unsigned long lockoutUntil  = 0;
unsigned long redFlashUntil = 0;
unsigned long wrongUntil    = 0;
unsigned long lastShownSec  = 0;

int  servoAngle   = ANGLE_LOCKED;
int  servoTarget  = ANGLE_LOCKED;
unsigned long lastServoMove = 0;

unsigned long beepUntil = 0;
bool beeping = false;

// ============================ Nguyen mau ham ============================
void loadPin();
void savePin();
void startBeep(int freq, unsigned long ms);
void updateBeep();
void setServoTarget(int target);
void updateServo();
void updateLeds();
void showLockedScreen();
void showChangeScreen(const char* prompt);
void showWrongScreen();
void showLockoutScreen();
void renderEntry();
void clearEntry();
const char* changePrompt();
void handleKey(char key);
void processEntry();
void verifyAndUnlock();
void unlock();
void relock();

// ============================ setup ============================
void setup() {
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  loadPin();

  servo.attach(PIN_SERVO);
  servo.write(ANGLE_LOCKED);
  servoAngle = ANGLE_LOCKED;
  servoTarget = ANGLE_LOCKED;

  lcd.init();
  lcd.backlight();
  showLockedScreen();

  startBeep(1200, 80);
}

// ============================ loop ============================
void loop() {
  updateBeep();
  updateServo();
  updateLeds();

  char key = keypad.getKey();
  if (key != NO_KEY) {
    startBeep(1000, 40);
    handleKey(key);
  }

  switch (state) {
    case S_LOCKED:
      if (wrongUntil && millis() >= wrongUntil) {
        wrongUntil = 0;
        clearEntry();
        showLockedScreen();
      }
      break;

    case S_UNLOCKED: {
      if (millis() >= unlockAt) {
        relock();
        break;
      }
      unsigned long remain = (unlockAt - millis() + 999UL) / 1000UL;
      if (remain != lastShownSec) {
        lastShownSec = remain;
        lcd.setCursor(12, 1);
        lcd.print("  ");
        lcd.setCursor(12, 1);
        lcd.print(remain);
        lcd.setCursor(14, 1);
        lcd.print('s');
      }
      break;
    }

    case S_LOCKOUT: {
      if (millis() >= lockoutUntil) {
        attempts = 0;
        state = S_LOCKED;
        clearEntry();
        showLockedScreen();
        break;
      }
      unsigned long remain = (lockoutUntil - millis() + 999UL) / 1000UL;
      if (remain != lastShownSec) {
        lastShownSec = remain;
        lcd.setCursor(8, 1);
        lcd.print("   ");
        lcd.setCursor(8, 1);
        lcd.print(remain);
        lcd.setCursor(11, 1);
        lcd.print('s');
      }
      break;
    }

    default:
      break;
  }
}

// ============================ Xu ly phim ============================
void handleKey(char key) {
  if (state == S_LOCKOUT) return;

  if (state == S_UNLOCKED) {
    if (key == 'D') relock();   // khoa tay
    return;
  }

  if (state == S_LOCKED && wrongUntil) {
    wrongUntil = 0;
    clearEntry();
    showLockedScreen();
  }

  if (key == '*') {
    clearEntry();
    if (state == S_LOCKED) showLockedScreen();
    else showChangeScreen(changePrompt());
    return;
  }

  if (key == '#') {
    if (state == S_LOCKED) {
      state = S_CHANGE_VERIFY;
      clearEntry();
      showChangeScreen("OLD PIN:");
    } else if (state >= S_CHANGE_VERIFY && state <= S_CHANGE_CONFIRM) {
      state = S_LOCKED;         // huy che do doi mat khau
      clearEntry();
      showLockedScreen();
    }
    return;
  }

  if (key >= '0' && key <= '9' && entryLen < PIN_LEN) {
    entry[entryLen++] = key;
    entry[entryLen] = '\0';
    renderEntry();
    if (entryLen == PIN_LEN) processEntry();
  }
}

const char* changePrompt() {
  switch (state) {
    case S_CHANGE_VERIFY:  return "OLD PIN:";
    case S_CHANGE_NEW:     return "NEW PIN:";
    case S_CHANGE_CONFIRM: return "CONFIRM:";
    default:               return "";
  }
}

void processEntry() {
  switch (state) {
    case S_LOCKED:
      verifyAndUnlock();
      break;

    case S_CHANGE_VERIFY:
      if (strcmp(entry, masterPin) == 0) {
        state = S_CHANGE_NEW;
        startBeep(1000, 80);
        clearEntry();
        showChangeScreen("NEW PIN:");
      } else {
        startBeep(200, 250);
        state = S_LOCKED;
        clearEntry();
        showLockedScreen();
      }
      break;

    case S_CHANGE_NEW:
      strcpy(newPin, entry);
      state = S_CHANGE_CONFIRM;
      clearEntry();
      showChangeScreen("CONFIRM:");
      break;

    case S_CHANGE_CONFIRM:
      if (strcmp(entry, newPin) == 0) {
        strcpy(masterPin, newPin);
        savePin();
        startBeep(1500, 120);
        state = S_LOCKED;
        clearEntry();
        showLockedScreen();
      } else {
        startBeep(200, 250);
        state = S_CHANGE_NEW;
        clearEntry();
        showChangeScreen("NEW PIN:");
      }
      break;

    default:
      break;
  }
}

// ============================ Mo/khoa ============================
void verifyAndUnlock() {
  if (strcmp(entry, masterPin) == 0) {
    unlock();
  } else {
    attempts++;
    redFlashUntil = millis() + RED_FLASH_MS;
    startBeep(200, 300);
    if (attempts >= MAX_ATTEMPTS) {
      state = S_LOCKOUT;
      clearEntry();
      lockoutUntil = millis() + LOCKOUT_MS;
      lastShownSec = 0;
      showLockoutScreen();
    } else {
      wrongUntil = millis() + WRONG_MSG_MS;
      showWrongScreen();
    }
  }
}

void unlock() {
  attempts = 0;
  state = S_UNLOCKED;
  clearEntry();
  redFlashUntil = 0;
  unlockAt = millis() + AUTO_RELOCK_MS;
  lastShownSec = 0;
  setServoTarget(ANGLE_UNLOCKED);
  startBeep(1500, 100);
  digitalWrite(PIN_LED_GREEN, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UNLOCKED");
  lcd.setCursor(0, 1);
  lcd.print("Auto lock 5s");
  lcd.setCursor(12, 1);
  lcd.print(AUTO_RELOCK_MS / 1000UL);
  lcd.setCursor(14, 1);
  lcd.print('s');
}

void relock() {
  state = S_LOCKED;
  setServoTarget(ANGLE_LOCKED);
  digitalWrite(PIN_LED_GREEN, LOW);
  showLockedScreen();
}

// ============================ Man hinh LCD ============================
void showLockedScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENTER PIN:");
  renderEntry();
}

void showChangeScreen(const char* prompt) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  renderEntry();
}

void showWrongScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WRONG PIN!");
  lcd.setCursor(0, 1);
  lcd.print(MAX_ATTEMPTS - attempts);
  lcd.print(" attempt(s) left");
}

void showLockoutScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LOCKED OUT!");
  lcd.setCursor(0, 1);
  lcd.print("Wait 30s");
}

void renderEntry() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  for (byte i = 0; i < entryLen; i++) lcd.print('*');
}

void clearEntry() {
  entryLen = 0;
  entry[0] = '\0';
}

// ============================ EEPROM ============================
void loadPin() {
  for (int i = 0; i < PIN_LEN; i++) {
    char c = EEPROM.read(EEPROM_ADDR + i);
    if (c >= '0' && c <= '9') masterPin[i] = c;
  }
  masterPin[PIN_LEN] = '\0';
}

void savePin() {
  for (int i = 0; i < PIN_LEN; i++) {
    EEPROM.write(EEPROM_ADDR + i, masterPin[i]);
  }
}

// ============================ Buzzer (khong blocking) ============================
void startBeep(int freq, unsigned long ms) {
  beeping = true;
  tone(PIN_BUZZER, freq);
  beepUntil = millis() + ms;
}

void updateBeep() {
  if (beeping && millis() >= beepUntil) {
    noTone(PIN_BUZZER);
    beeping = false;
  }
}

// ============================ Servo (quay tu tu) ============================
void setServoTarget(int target) {
  servoTarget = target;
}

void updateServo() {
  if (servoAngle == servoTarget) return;
  if (millis() - lastServoMove < SERVO_STEP_MS) return;
  lastServoMove = millis();
  if (servoAngle < servoTarget) servoAngle++;
  else servoAngle--;
  servo.write(servoAngle);
}

// ============================ LED ============================
void updateLeds() {
  if (state == S_LOCKOUT) {
    digitalWrite(PIN_LED_RED, (millis() / 500) % 2);
  } else {
    digitalWrite(PIN_LED_RED, millis() < redFlashUntil ? HIGH : LOW);
  }
}