//flash counter
//matrix control
#include <LedControl.h>
#include <EEPROM.h>

//ONLY pins 2 and 3 have interrups
#define SENSOR_PIN 2 
#define BUTTON_PIN 3 
#define LIGHT_PIN  A0  // 

//pins for
LedControl matrix(9, 11, 10, 1);  // DIN, CLK, CS, how many matrices

//EEPROM https://docs.arduino.cc/learn/programming/eeprom-guide/

//bare metal so checmsum etc are kinda good
// magic = is our data
#define MAGIC   0xF1A5
// differenet structs if ver mismatch need to overwrite
#define VERSION 3

//sctructs cleanest for eeprom starts from 0 as an array.
struct Saved {
  unsigned int magic;
  byte version;
  unsigned long count;
  byte brightness;
};
Saved data;
bool changed = false;  // set when count or brightness.

//state machine
enum State { SLEEP, AWAKE, FADING };
State state = SLEEP;

unsigned long ticks = 0;        // 10 ms steps since boot
unsigned long lastActivity = 0;
byte fadeLevel = 0;

//debouncing
byte darkFor = 0;               // ticks the sensor has been dark
byte buttonUpFor = 0;           // ticks the button has been released


//https://www.gammon.com.au/forum/threads/11488.html
/* ompiler must reload the variable whenever you reference it, rather than relying upon a copy it might have in a processor register
Always rereads from memory instead of using stored values in registers*/

volatile bool tick = false;     // Timer1, every 10 ms
// interrupt flags
volatile bool flashed = false;  // pin 2 went HIGH
volatile bool pressed = false;  // pin 3 went LOW

//TIMER1 compare match value interrupt vector
ISR(TIMER1_COMPA_vect)
{
  tick = true;
}

//set flags
void flashISR()
{
  flashed = true;
}
void buttonISR()
{
  pressed = true;
}


// 3x5 digits, one row per number, 3 bits wide 0-9
const byte FONT[10][5] = {
  {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
  {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};

// rotate 90 degrees so its clean since we put it in rotater
void setPixel(byte row, byte col) {
  matrix.setLed(0, col, 7 - row, true);
}

//takes int 0-7 converts to binary eg 111, led on for each 1. 3x5 digit.
void drawDigit(byte d, byte x) {
  for (byte r = 0; r < 5; r++)
    for (byte c = 0; c < 3; c++) //reads the number in font as binary eg 100 -> set pixel x+1, x+2, x+3. 
      if (bitRead(FONT[d][r], 2 - c)) setPixel(r + 1, x + c);
}

//draws the digits count
void showCount() {
  byte n = data.count % 100;  // only two digits fit, get current count from our data
  matrix.clearDisplay(0);
  if (n >= 10) {
    drawDigit(n / 10, 0); //tens drawn to left
    drawDigit(n % 10, 4); // drawn to right +4 
  } else { // if one digit
    drawDigit(n, 2);
  }
}

// what happends it becomes active.
void wakeUp() {
  lastActivity = ticks;
  state = AWAKE;
  matrix.shutdown(0, false); //shutdown == false means its awake
  matrix.setIntensity(0, data.brightness);
  showCount();
}


void loadData() {
  //puts eeprom values to data, if the data is wrong puts in defaults
  EEPROM.get(0, data);
  if (data.magic != MAGIC || data.version != VERSION) {
    Serial.println("EEPROM empty or old -> defaults");
    data = {MAGIC, VERSION, 0, 1};
    EEPROM.put(0, data);
  }
}

/* The purpose of this example is to show the
EEPROM.put()
method that writes data on EEPROM using also the EEPROM.update() that writes data only if it is different from the previous content of the locations to be*/
//the changed is uselss put already only changes changes items. write - on flag
void saveIfChanged() {
  if (!changed) return;   // write-on-change: only save if count/brightness changed
  EEPROM.put(0, data);
  changed = false;
  Serial.println("saved to EEPROM");
}

// Timer1 in CTC (compare to clear)mode: 16 MHz / 256 = 62500 Hz, 625 counts = 10 ms
//https://www.gammon.com.au/forum/threads/11504.html?id=11504
// TCNT1  = Timer/CouNTer 1: the count itself
// TCCR1A = Timer/Counter Control Register 1A: settings part 1 (WGM10, WGM11) - all 0 for CTC
// TCCR1B = Timer/Counter Control Register 1B: settings part 2 (WGM12, WGM13, prescaler CS10-12)
// OCR1A  = Output Compare Register 1A: number to count to
// TIMSK1 = Timer Interrupt MaSK 1: which timer interrupts are on (OCIE1A)
// WGM    = Waveform Generation Mode, CTC = Clear Timer on Compare
// CS     = Clock Select (prescaler), OCIE1A = Output Compare Interrupt Enable 1A
void setupTimer() {
  TCCR1A = 0;              // clear all settings
  TCCR1B = 0;
  bitSet(TCCR1B, WGM12);   // CTC mode: count to OCR1A, then restart at 0
  
  //prescalers so it doesnt overflow, so we count all the MHz type stuff.
  bitSet(TCCR1B, CS12);    // prescaler 256: 16 MHz / 256 = 62500 counts/s
  OCR1A = 624;             // 0..624 = 625 counts = 10 ms
  bitSet(TIMSK1, OCIE1A);  // run the interrupt each time it reaches OCR1A
}

void setup() {
  Serial.begin(9600);
  loadData();
  Serial.print("boot, count = ");
  Serial.println(data.count);

  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  delay(100);  // let the matrix power up
  matrix.setScanLimit(0, 7);

  setupTimer();
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), flashISR, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  wakeUp();
}

void loop() {
  // flash only counts if the sensor was dark for 100 ms
  if (flashed) {
    flashed = false;
    if (darkFor >= 10) {
      darkFor = 0;
      data.count++;
      changed = true;
      Serial.print("flash! count = ");
      Serial.println(data.count);
      wakeUp();
    }
  }

  // button only counts if it was released for 50 ms before decoucning, 5 timer ticks
  if (pressed) {
    pressed = false;
    if (buttonUpFor >= 5) {
      buttonUpFor = 0;
      if (state == AWAKE) {  // cycle brightness 1 -> 2 -> 4 -> 8 -> 1
        data.brightness *= 2;                          // double it
        if (data.brightness > 8) data.brightness = 1;  // past max -> back to 1
        changed = true;
        Serial.print("brightness = ");
        Serial.println(data.brightness);
      }
      wakeUp();
    }
  }

  if (tick) {
    tick = false;
    ticks++;

    if (digitalRead(SENSOR_PIN) == LOW && darkFor < 255) darkFor++;
    if (digitalRead(BUTTON_PIN) == HIGH && buttonUpFor < 255) buttonUpFor++;
    if (digitalRead(BUTTON_PIN) == LOW) buttonUpFor = 0;

    if (ticks % 100 == 0) {  //every 1s logging
      Serial.print("light = ");
      Serial.println(analogRead(LIGHT_PIN));
    }

    if (state == AWAKE && ticks - lastActivity > 500) {  // 5 s idle
      state = FADING;
      fadeLevel = data.brightness;
      saveIfChanged();
    }

    if (state == FADING && ticks % 15 == 0) {  // one step every 150 ms
      if (fadeLevel == 0) {
        matrix.shutdown(0, true);
        state = SLEEP;
      } else {
        matrix.setIntensity(0, --fadeLevel);
      }
    }
  }
}
