
//
// # The controller of the boiler
//
//
// ## Connections:
// DS1307:  SDA       -> A4
//          SCL       -> A5
// LCD:     SDA       -> A4
//          SCL       -> A5
// DS18B20: data      -> D10

// Ночная зона: с 23:00 до 7:00 часов
// Обычная зона: с 10:00 до 17:00 часов, с 21:00 до 23:00 часов
// Пиковая зона: с 7:00 до 10:00 часов, с 17:00 до 21:00 часов

// TODO: Отключать монитор после 15 секунд бездействия
// TODO: Режим редактирования времени и погрешности (5-30 минут)

#include <Wire.h>
#include <OneWire.h>
#include <LiquidCrystal_I2C.h>
#include <RtcDS1307.h>


OneWire ds (10);
byte data[12];
byte tempAddr[8] = {0x28, 0xFF, 0x41, 0xD7, 0x43, 0x16, 0x03, 0x9A};
unsigned int raw;
float temp;

LiquidCrystal_I2C LCD(0x27, 20, 4);
RtcDS1307 RTC;

int pinLedGreen = 11;
int pinLedYellow = 12;
int pinLedRed = 13;

int pinRelay = 8;

int pinAxisY = 0;
int pinAxisX = 1;
int pinAxisZ = 7;
int value_X, value_Y, value_Z = 0;

String releController;

int tumblerOn = 2;
int tumblerOff = 3;

int accuracyPlace = 0;
int accuracy = 15;
int TimeZoneAccuracy, TimeZoneAccuracyInvert;

void setup() {
  Serial.begin(9600);
  LCD.init();
  LCD.backlight();
  RTC.Begin();

  pinMode(pinLedGreen, OUTPUT);
  pinMode(pinLedYellow, OUTPUT);
  pinMode(pinLedRed, OUTPUT);

  pinMode(pinRelay, OUTPUT);

  pinMode(pinAxisZ, INPUT_PULLUP);

  pinMode(tumblerOn, INPUT_PULLUP);
  pinMode(tumblerOff, INPUT_PULLUP);

  TimeZoneAccuracy = RTC.GetMemory(accuracyPlace);
  if (!TimeZoneAccuracy) {
    RTC.SetMemory(0, accuracy);
    TimeZoneAccuracy = accuracy;
  }

  TimeZoneAccuracyInvert = 60 - TimeZoneAccuracy;

  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL //TODO: TEST IT

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.print("compiled: ");
  Serial.println(getDateTime(compiled));

  if (!RTC.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
    RTC.SetDateTime(compiled);
  }

  if (!RTC.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    RTC.SetIsRunning(true);
  }

  RtcDateTime now = RTC.GetDateTime();

  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    RTC.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  RTC.SetSquareWavePin(DS1307SquareWaveOut_Low);
}

void loop() {
  value_X = analogRead(pinAxisX);    // Считываем аналоговое значение оси Х
  Serial.print("X:");
  Serial.print(value_X);      // Выводим значение в Serial Monitor

  value_Y = analogRead(pinAxisY);    // Считываем аналоговое значение оси Y
  Serial.print(" | Y:");
  Serial.print(value_Y, DEC);      // Выводим значение в Serial Monitor

  value_Z = digitalRead(pinAxisZ);   // Считываем цифровое значение оси Z (кнопка)
  value_Z = value_Z ^ 1; // инвертируем значение
  Serial.print(" | Z: ");
  Serial.println(value_Z);

  LCD.setCursor(15, 3);
  if (!digitalRead(tumblerOn)) {
    releController = "on";
    LCD.print("*");
  } else if (!digitalRead(tumblerOff)) {
    releController = "off";
    LCD.print("*");
  } else {
    releController = "auto";
    LCD.print(" ");
  }

  if (!RTC.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = RTC.GetDateTime();

  LCD.setCursor(0, 0);
  LCD.print(getDateTime(now));
  LCD.setCursor(18, 0);
  switch (now.DayOfWeek()) {
    case 1: LCD.print("nH"); break;
    case 2: LCD.print("BT"); break;
    case 3: LCD.print("CP"); break;
    case 4: LCD.print("4T"); break;
    case 5: LCD.print("nT"); break;
    case 6: LCD.print("C6"); break;
    case 7: LCD.print("BC"); break;
  }
  Serial.println(getDateTime(now)); // TODO: only for test

  temp = DS18B20(tempAddr);

  LCD.setCursor(0, 2);
  LCD.print("3OHA  +- TEMP  PEJIE");
  LCD.setCursor(6, 3);
  LCD.print(TimeZoneAccuracy);
  LCD.setCursor(9, 3);
  LCD.print(temp, 1);

  String nowTime = formatTime(now.Hour(), now.Minute());
  if (formatTime(6, TimeZoneAccuracyInvert) <= nowTime && nowTime < formatTime(10, TimeZoneAccuracy) ||
      formatTime(16, TimeZoneAccuracyInvert) <= nowTime && nowTime < formatTime(21, TimeZoneAccuracy)) {
    // Expensive zone - 1
    // 6:45 - 10:14
    // 16:45 - 21:14
    doControl(1);
  } else if (formatTime(10, TimeZoneAccuracy) <= nowTime && nowTime < formatTime(16, TimeZoneAccuracyInvert) ||
             formatTime(21, TimeZoneAccuracy) <= nowTime && nowTime < formatTime(23, TimeZoneAccuracy)) {
    // Normal zone - 2
    // 10:15 - 16:44
    // 21:15 - 23:14
    doControl(2);
  } else if (formatTime(23, TimeZoneAccuracy) <= nowTime || nowTime < formatTime(6, TimeZoneAccuracyInvert)) {
    // Cheap zone - 3
    // 23:15 - 6:44
    doControl(3);
  }
  delay (1000);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

String getDateTime(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(
    datestring,
    countof(datestring),
    PSTR("%02u/%02u/%02u %02u:%02u:%02u"),
    dt.Day(),
    dt.Month(),
    dt.Year()-2000,
    dt.Hour(),
    dt.Minute(),
    dt.Second()
  );

  return datestring;
}

String formatTime(int hour, int minute) {
  char timestring[6];

  snprintf_P(
    timestring,
    countof(timestring),
    PSTR("%02u:%02u"),
    hour,
    minute
  );

  return timestring;
}

void doControl(int mode) {
  LCD.setCursor(0, 3);
  switch (mode) {
    case 1:
      LCD.print("$$$");
      digitalWrite(pinLedRed, HIGH);
      digitalWrite(pinLedYellow, LOW);
      digitalWrite(pinLedGreen, LOW);

      LCD.setCursor(17, 3);
      if (releController == "on") {
        LCD.print(" ON");
        digitalWrite(pinRelay, LOW);
      } else {
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      }
      break;
    case 2:
      LCD.print("$$ ");
      digitalWrite(pinLedRed, LOW);
      digitalWrite(pinLedYellow, HIGH);
      digitalWrite(pinLedGreen, LOW);

      LCD.setCursor(17, 3);
      if (releController == "on") {
        LCD.print(" ON");
        digitalWrite(pinRelay, LOW);
      } else if (releController == "off") {
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      } else if (temp < 40) {
        LCD.print(" ON");
        digitalWrite(pinRelay, LOW);
      } else {
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      }
      break;
    default:
      LCD.print("$  ");
      digitalWrite(pinLedRed, LOW);
      digitalWrite(pinLedYellow, LOW);
      digitalWrite(pinLedGreen, HIGH);

      LCD.setCursor(17, 3);
      if (releController == "on") {
        LCD.print(" ON");
        digitalWrite(pinRelay, LOW);
      } else if (releController == "off") {
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      } else if (temp < 80) {
        LCD.print(" ON");
        digitalWrite(pinRelay, LOW);
      } else {
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      }
  }
}

float DS18B20(byte *adres) {
  ds.reset();
  ds.select(adres);
  ds.write(0x44, 1);
  ds.reset();
  ds.select(adres);
  ds.write(0xBE);

  for (byte i = 0; i < 12; i++) {
    data[i] = ds.read ();
  }
  raw = (data[1] << 8) | data[0];
  float celsius = (float)raw / 16.0;
  return celsius;
}
