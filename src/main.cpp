#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Preferences.h>

Preferences preferences;

// ==========================================
// PIN DEFINITIONS (Đã cập nhật theo yêu cầu)
// ==========================================
#define SDA_PIN 8
#define SCL_PIN 9
#define RELAY_PIN 10
#define BUZZER_PIN 47   // Buzzer đổi sang GPIO 47
#define LED_GREEN 20    // LED Green đổi sang GPIO 20
#define LED_RED 19      // LED Red đổi sang GPIO 19

// ==========================================
// KEYPAD CONFIGURATION
// ==========================================
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {4, 5, 6, 7};
byte colPins[COLS] = {15, 16, 17, 18};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ==========================================
// LCD CONFIGURATION
// ==========================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================================
// SYSTEM VARIABLES & STATES
// ==========================================
String currentInput = "";
String savedPassword = "1234";
int failedAttempts = 0;

enum SystemState {
  STATE_IDLE,
  RESET_AUTH,
  RESET_NEW
};
SystemState state = STATE_IDLE;

// Khai báo trước các hàm
void refreshScreen();
void processInput();
void resetToIdle();

// ==========================================
// MAIN SETUP
// ==========================================
void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  // Đọc mật khẩu đã lưu từ bộ nhớ Flash của ESP32
  preferences.begin("lock_sys", false);
  savedPassword = preferences.getString("password", "1234");

  lcd.clear();
  lcd.print("System Ready");
  delay(1000);
  resetToIdle();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  char key = keypad.getKey();

  if (key) {
    // Xóa ký tự khi bấm '*'
    if (key == '*' && currentInput.length() > 0) {
      currentInput.remove(currentInput.length() - 1);
      refreshScreen();
      return;
    }

    // Nhập số (0-9)
    if (key >= '0' && key <= '9') {
      if (currentInput.length() < 4) {
        currentInput += key;
        refreshScreen();
      }

      // Tự động kiểm tra khi đủ 4 chữ số
      if (currentInput.length() == 4) {
        processInput();
      }
    }

    // Bấm 'A' ở màn hình chờ để đổi mật khẩu
    if (key == 'A' && state == STATE_IDLE) {
      state = RESET_AUTH;
      currentInput = "";
      lcd.clear();
      lcd.print("Enter Old PIN:");
      lcd.setCursor(0, 1);
    }
  }
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================
void refreshScreen() {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  for (int i = 0; i < currentInput.length(); i++) {
    lcd.print("*");
  }
}

void processInput() {
  // Trạng thái chờ nhập PIN
  if (state == STATE_IDLE) {
    if (currentInput == savedPassword) {
      failedAttempts = 0;
      lcd.clear();
      lcd.print("Access Granted");
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(LED_GREEN, HIGH);

      delay(3000); // Mở cửa trong 3 giây

      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(LED_GREEN, LOW);
      resetToIdle();
    } else {
      failedAttempts++;
      lcd.clear();
      lcd.print("Wrong PIN!");
      digitalWrite(LED_RED, HIGH);

      if (failedAttempts >= 3) {
        lcd.setCursor(0, 1);
        lcd.print("LOCKED OUT!");
        digitalWrite(BUZZER_PIN, HIGH);
        delay(5000); // Khóa 5 giây nếu nhập sai 3 lần
        digitalWrite(BUZZER_PIN, LOW);
        failedAttempts = 0;
      } else {
        delay(3000);
      }
      digitalWrite(LED_RED, LOW);
      resetToIdle();
    }
  }

  // Xác thực mật khẩu cũ để đổi PIN mới
  else if (state == RESET_AUTH) {
    if (currentInput == savedPassword) {
      state = RESET_NEW;
      currentInput = "";
      lcd.clear();
      lcd.print("Enter New PIN:");
      lcd.setCursor(0, 1);
    } else {
      lcd.clear();
      lcd.print("Auth Failed!");
      digitalWrite(LED_RED, HIGH);
      delay(3000);
      digitalWrite(LED_RED, LOW);
      resetToIdle();
    }
  }

  // Lưu mật khẩu mới vào bộ nhớ Flash vĩnh viễn
  else if (state == RESET_NEW) {
    savedPassword = currentInput;
    preferences.putString("password", savedPassword);

    lcd.clear();
    lcd.print("PIN Saved!");
    digitalWrite(LED_GREEN, HIGH);
    delay(2000);
    digitalWrite(LED_GREEN, LOW);
    resetToIdle();
  }
}

void resetToIdle() {
  state = STATE_IDLE;
  currentInput = "";
  lcd.clear();
  lcd.print("Enter PIN:");
  lcd.setCursor(0, 1);
}