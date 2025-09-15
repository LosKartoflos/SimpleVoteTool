#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RCSwitch.h>

#define LCD_ADDR 0x27      // ggf. 0x3F anpassen
#define LCD_COLS 20
#define LCD_ROWS 4

const int RC_PIN = 0;      // Interrupt 0 = Pin 2 am UNO
const int BEEPER_PIN = 4;

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
RCSwitch mySwitch;

const int MAX_ID = 100;

struct RemoteState {
  int8_t p1; // -1 = keine Stimme, 0 = NEIN, 1 = JA
  int8_t p2;
  bool used;
};

RemoteState remotes[MAX_ID + 1];

bool frozen = false;
bool showLastResult = false;

int lastYes = 0, lastNo = 0, lastUsed = 0; // gespeichertes Ergebnis vorm Reset
unsigned long lastReceived = 0;
unsigned long lastValue = 0;
const unsigned long DUPLICATE_IGNORE_MS = 200;

// Hilfsfunktionen
void beep(int ms = 80) {
  digitalWrite(BEEPER_PIN, HIGH);
  delay(ms);
  digitalWrite(BEEPER_PIN, LOW);
}

void saveLastResult() {
  lastYes = 0; lastNo = 0; lastUsed = 0;
  for (int i = 1; i <= MAX_ID; i++) {
    if (remotes[i].p1 != -1 || remotes[i].p2 != -1) lastUsed++;
    if (remotes[i].p1 == 1) lastYes++; else if (remotes[i].p1 == 0) lastNo++;
    if (remotes[i].p2 == 1) lastYes++; else if (remotes[i].p2 == 0) lastNo++;
  }
}

void resetAllVotes() {
  saveLastResult();
  for (int i = 1; i <= MAX_ID; i++) {
    remotes[i].p1 = -1;
    remotes[i].p2 = -1;
    remotes[i].used = false;
  }
  updateDisplay();
}

void toggleFreeze() {
  frozen = !frozen;
  updateDisplay();
}

void toggleShowLast() {
  showLastResult = !showLastResult;
  updateDisplay();
}

void computeTotals(int &yes, int &no, int &used) {
  yes = 0; no = 0; used = 0;
  for (int i = 1; i <= MAX_ID; i++) {
    if (remotes[i].p1 != -1 || remotes[i].p2 != -1) used++;
    if (remotes[i].p1 == 1) yes++; else if (remotes[i].p1 == 0) no++;
    if (remotes[i].p2 == 1) yes++; else if (remotes[i].p2 == 0) no++;
  }
}

void updateDisplay() {
  int totalYes, totalNo, usedRemotes;
  if (showLastResult) {
    totalYes = lastYes;
    totalNo = lastNo;
    usedRemotes = lastUsed;
  } else {
    computeTotals(totalYes, totalNo, usedRemotes);
  }

  int totalVotes = totalYes + totalNo;
  int percentYes = (totalVotes > 0) ? (totalYes * 100 / totalVotes) : 0;
  int percentNo  = (totalVotes > 0) ? (totalNo  * 100 / totalVotes) : 0;

  char buf[LCD_COLS + 1];

  // Zeile 0
  lcd.setCursor(0,0);
  snprintf(buf, sizeof(buf), "Votes Y:%2d N:%2d", totalYes, totalNo);
  lcd.print(buf);
  for (int i = strlen(buf); i < LCD_COLS; i++) lcd.print(" ");

  // Zeile 1
  lcd.setCursor(0,1);
  snprintf(buf, sizeof(buf), "Ja%%:%3d  Nein%%:%3d", percentYes, percentNo);
  lcd.print(buf);
  for (int i = strlen(buf); i < LCD_COLS; i++) lcd.print(" ");

  // Zeile 2
  lcd.setCursor(0,2);
  snprintf(buf, sizeof(buf), "Remotes: %3d/%3d", usedRemotes, MAX_ID);
  lcd.print(buf);
  for (int i = strlen(buf); i < LCD_COLS; i++) lcd.print(" ");

  // Zeile 3 (Status)
  lcd.setCursor(0,3);
  if (showLastResult) {
    lcd.print("Letztes Ergebnis ");
  } else {
    if (frozen) lcd.print("Status: Eingefroren");
    else        lcd.print("Status: Aktiv     ");
  }
}

void setup() {
  pinMode(BEEPER_PIN, OUTPUT);
  digitalWrite(BEEPER_PIN, LOW);

  Wire.begin();
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Abstimmung Tool");
  delay(600);

  mySwitch.enableReceive(0); // Interrupt 0 = Pin 2

  for (int i = 0; i <= MAX_ID; i++) {
    remotes[i].p1 = -1;
    remotes[i].p2 = -1;
    remotes[i].used = false;
  }

  updateDisplay();
}

void loop() {
  if (mySwitch.available()) {
    unsigned long value = mySwitch.getReceivedValue();
    mySwitch.resetAvailable();

    if (value == 0) return;

    unsigned long now = millis();
    if (value == lastValue && (now - lastReceived) < DUPLICATE_IGNORE_MS) return;
    lastValue = value;
    lastReceived = now;

    int button = value % 10;
    int id = (value / 10) % 1000;

    if (id < 0 || id > MAX_ID) return;

    if (id == 0) { // Master
      if (button == 0) { toggleFreeze(); beep(100); }
      else if (button == 1) { resetAllVotes(); beep(120); }
      else if (button == 2) { toggleShowLast(); beep(80); }
      return;
    }

    if (id >= 1 && id <= MAX_ID) {
      if (frozen) return;

      bool stateChanged = false;

      if (button == 0 && remotes[id].p1 != 1) { remotes[id].p1 = 1; stateChanged = true; }
      else if (button == 1 && remotes[id].p1 != 0) { remotes[id].p1 = 0; stateChanged = true; }
      else if (button == 2 && remotes[id].p2 != 1) { remotes[id].p2 = 1; stateChanged = true; }
      else if (button == 3 && remotes[id].p2 != 0) { remotes[id].p2 = 0; stateChanged = true; }

      if (stateChanged) {
        remotes[id].used = true;
        beep(70);
        if (!showLastResult) updateDisplay();
      }
    }
  }

  delay(10);
}
