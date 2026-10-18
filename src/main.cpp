  #include <Arduino.h>
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  #include <Keypad.h>
  #include <Preferences.h>

  #define I2C_SDA_PIN 8
  #define I2C_SCL_PIN 9

  #define RELAY_PIN 10
  #define BUZZER_PIN 11
  #define GREEN_LED_PIN 47
  #define RED_LED_PIN 21

  #define RELAY_ON HIGH
  #define RELAY_OFF LOW

  const unsigned long PAUSE_3S_MS = 3000;
  const unsigned long LOCKOUT_5S_MS = 5000;
  const unsigned long RELAY_UNLOCK_MS = 5000;
  const unsigned long DEBOUNCE_TIME_MS = 60;

  const int PASSWORD_LENGTH = 4;
  const int MAX_TRIES = 3;

  LiquidCrystal_I2C lcd(0x27, 16, 2);
  Preferences preferences;

  const byte ROWS = 4;
  const byte COLS = 4;

  char keys[ROWS][COLS] = {
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'}};

  byte rowPins[ROWS] = {4, 5, 6, 7};
  byte colPins[COLS] = {15, 16, 17, 18};

  Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

  enum SystemState
  {
    STATE_STANDBY,
    STATE_INPUTTING,
    STATE_CHECKING,
    STATE_PAUSE_3S,
    STATE_UNLOCKED,
    STATE_LOCKED_OUT_5S,
    STATE_VERIFY_OLD,
    STATE_SET_PASSWORD
  };

  SystemState system_state = STATE_STANDBY;
  SystemState next_state = STATE_STANDBY;

  char saved_PIN[PASSWORD_LENGTH + 1] = "1234";
  char entered_PIN[PASSWORD_LENGTH + 1] = "";
  int digit_count = 0;
  int failed_tries = 0;

  unsigned long timer_start_ms = 0;
  int last_shown_sec = -1;
  String pause_message = "";

  void beep(int ms = 60)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(ms);
    digitalWrite(BUZZER_PIN, LOW);
  }

  void loadPassword()
  {
    preferences.begin("security_app", false);
    bool hasPassword = preferences.getBool("initialized", false);
    if (hasPassword)
    {
      String pin = preferences.getString("pin", "1234");
      pin.toCharArray(saved_PIN, PASSWORD_LENGTH + 1);
      system_state = STATE_STANDBY;
    }
    else
    {
      system_state = STATE_SET_PASSWORD;
    }
  }

  void savePassword(const char *newPin)
  {
    strncpy(saved_PIN, newPin, PASSWORD_LENGTH);
    saved_PIN[PASSWORD_LENGTH] = '\0';
    preferences.putString("pin", saved_PIN);
    preferences.putBool("initialized", true);
  }

  void clearTypedInput()
  {
    digit_count = 0;
    for (int i = 0; i <= PASSWORD_LENGTH; i++)
    {
      entered_PIN[i] = '\0';
    }
  }

  void flushKeypad()
  {
    while (keypad.getKey() != NO_KEY)
    {
    }
  }
  void updateLCDScreen()
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    if (system_state == STATE_SET_PASSWORD)
    {
      lcd.print("SET 4-DIGIT PIN:");
    }
    else if (system_state == STATE_VERIFY_OLD)
    {
      lcd.print("ENTER OLD PIN:");
    }
    else
    {
      lcd.print("ENTER PASSWORD");
    }

    lcd.setCursor(0, 1);
    lcd.print("PIN: [");
    for (int i = 0; i < PASSWORD_LENGTH; i++)
    {
      if (i < digit_count)
      {
        lcd.print(entered_PIN[i]);
      }
      else
      {
        lcd.print("_");
      }
    }
    lcd.print("]");
  }

  void start3SecondPause(const char *title, SystemState targetState)
  {
    pause_message = String(title);
    next_state = targetState;
    timer_start_ms = millis();
    last_shown_sec = -1;
    system_state = STATE_PAUSE_3S;
  }

  void handleKeypadInput()
  {
    char key = keypad.getKey();
    if (key == NO_KEY)
      return;
    if (key >= '0' && key <= '9')
    {
      if (system_state == STATE_STANDBY)
      {
        system_state = STATE_INPUTTING;
      }
      if (digit_count < PASSWORD_LENGTH)
      {
        entered_PIN[digit_count] = key;
        digit_count++;
        entered_PIN[digit_count] = '\0';
        beep(50);
        updateLCDScreen();
      }
      else
      {
        beep(120);
      }
    }
    else if (key == 'C')
    {
      if (digit_count > 0)
      {
        digit_count--;
        entered_PIN[digit_count] = '\0';
        beep(40);
        if (digit_count == 0 && system_state == STATE_INPUTTING)
        {
          system_state = STATE_STANDBY;
        }
        updateLCDScreen();
      }
      else
      {
        beep(30);
      }
    }
    else if (key == '*')
    {
      beep(70);
      clearTypedInput();
      if (system_state == STATE_VERIFY_OLD)
      {
        system_state = STATE_STANDBY;
      }
      updateLCDScreen();
    }
    else if (key == '#')
    {
      if (digit_count < PASSWORD_LENGTH)
      {
        beep(150);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("NEED 4 DIGITS!");
        delay(1000);
        updateLCDScreen();
        return;
      }
      beep(60);
      if (system_state == STATE_SET_PASSWORD)
      {
        savePassword(entered_PIN);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PASSWORD SAVED!");
        lcd.setCursor(0, 1);
        lcd.print("NEW PIN: ");
        lcd.print(saved_PIN);
        beep(100);
        delay(1500);
        clearTypedInput();
        start3SecondPause("PAUSE 3S...", STATE_STANDBY);
      }
      else if (system_state == STATE_VERIFY_OLD)
      {
        if (strcmp(entered_PIN, saved_PIN) == 0)
        {
          beep(100);
          failed_tries = 0;
          clearTypedInput();
          system_state = STATE_SET_PASSWORD;
          updateLCDScreen();
        }
        else
        {
          system_state = STATE_CHECKING;
        }
      }
      else
      {
        system_state = STATE_CHECKING;
      }
    }
    else if (key == 'A' || key == 'B')
    {
      if (system_state == STATE_STANDBY || system_state == STATE_INPUTTING)
      {
        beep(70);
        clearTypedInput();
        system_state = STATE_VERIFY_OLD;
        updateLCDScreen();
      }
    }

    else if (key == 'D')
    {
      beep(300);
      delay(100);
      beep(300);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("   DOORBELL !   ");
      lcd.setCursor(0, 1);
      lcd.print(" Please wait... ");

      delay(1500);

      updateLCDScreen();
    }
  }

  void checkSubmittedPassword()
  {
    if (strcmp(entered_PIN, saved_PIN) == 0)
    {
      failed_tries = 0;
      clearTypedInput();
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RELAY_PIN, RELAY_ON);
      beep(100);
      delay(80);
      beep(150);
      timer_start_ms = millis();
      last_shown_sec = -1;
      system_state = STATE_UNLOCKED;
    }
    else
    {
      failed_tries++;
      clearTypedInput();
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
      int triesLeft = MAX_TRIES - failed_tries;
      if (triesLeft > 0)
      {
        for (int i = 0; i < 2; i++)
        {
          beep(120);
          delay(80);
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WRONG PASSWORD!");
        lcd.setCursor(0, 1);
        lcd.print("Tries left: ");
        lcd.print(triesLeft);
        delay(1200);
        start3SecondPause("PAUSE 3 SEC...", STATE_STANDBY);
      }
      else
      {
        digitalWrite(BUZZER_PIN, HIGH);
        timer_start_ms = millis();
        last_shown_sec = -1;
        system_state = STATE_LOCKED_OUT_5S;
      }
    }
  }

  void runUnlockedTimer()
  {
    unsigned long elapsed = millis() - timer_start_ms;
    if (elapsed >= RELAY_UNLOCK_MS)
    {
      digitalWrite(RELAY_PIN, RELAY_OFF);
      digitalWrite(GREEN_LED_PIN, LOW);
      start3SecondPause("LOCKING DOOR...", STATE_STANDBY);
      return;
    }
    int secLeft = (RELAY_UNLOCK_MS - elapsed + 999) / 1000;
    if (secLeft != last_shown_sec)
    {
      last_shown_sec = secLeft;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ACCESS GRANTED!");
      lcd.setCursor(0, 1);
      lcd.print("Relay ON: ");
      lcd.print(secLeft);
      lcd.print("s");
    }
  }

  void run5SecondLockoutTimer()
  {
    unsigned long elapsed = millis() - timer_start_ms;
    if (elapsed >= LOCKOUT_5S_MS)
    {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);
      failed_tries = 0;
      flushKeypad();
      start3SecondPause("PAUSE 3 SEC...", STATE_STANDBY);
      return;
    }
    int secLeft = (LOCKOUT_5S_MS - elapsed + 999) / 1000;
    if (secLeft != last_shown_sec)
    {
      last_shown_sec = secLeft;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("KEYPAD LOCKED!");
      lcd.setCursor(0, 1);
      lcd.print("Wait: ");
      lcd.print(secLeft);
      lcd.print(" seconds");
    }
  }

  void run3SecondPauseTimer()
  {
    unsigned long elapsed = millis() - timer_start_ms;
    if (elapsed >= PAUSE_3S_MS)
    {
      digitalWrite(RED_LED_PIN, LOW);
      flushKeypad();
      clearTypedInput();
      system_state = next_state;
      updateLCDScreen();
      return;
    }
    int secLeft = (PAUSE_3S_MS - elapsed + 999) / 1000;
    if (secLeft != last_shown_sec)
    {
      last_shown_sec = secLeft;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(pause_message);
      lcd.setCursor(0, 1);
      lcd.print("Wait: ");
      lcd.print(secLeft);
      lcd.print("s...");
    }
  }
  void setup()
  {
    Serial.begin(115200);

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);

    digitalWrite(RELAY_PIN, RELAY_OFF);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);

    keypad.setDebounceTime(DEBOUNCE_TIME_MS);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("SECURITY SYSTEM");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    beep(100);
    delay(1200);
    loadPassword();
    clearTypedInput();
    updateLCDScreen();
  }

  void loop()
  {
    switch (system_state)
    {
    case STATE_STANDBY:
    case STATE_INPUTTING:
    case STATE_VERIFY_OLD:
    case STATE_SET_PASSWORD:
      handleKeypadInput();
      break;
    case STATE_CHECKING:
      checkSubmittedPassword();
      break;
    case STATE_LOCKED_OUT_5S:
      run5SecondLockoutTimer();
      break;
    case STATE_PAUSE_3S:
      run3SecondPauseTimer();
      break;
    case STATE_UNLOCKED:
      runUnlockedTimer();
      break;
    }
  }