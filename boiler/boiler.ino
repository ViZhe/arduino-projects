
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
int joystickX, joystickY, joystickZ = 0;

String releController;

int tumblerOn = 2;
int tumblerOff = 3;

int accuracyPlace = 0;
int accuracy = 15;
int TimeZoneAccuracy, TimeZoneAccuracyInvert;

bool backlight = false;
int backlightTimeout = 0;

int editTimeMode = 0;

void setup() {
  Serial.begin(9600);
  LCD.init();
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
  }

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
  }

  RTC.SetSquareWavePin(DS1307SquareWaveOut_Low);

  LCD.setCursor(0, 2);
  LCD.print("3OHA  +- TEMP  PEJIE");
}

void loop() {
  TimeZoneAccuracy = RTC.GetMemory(accuracyPlace);
  TimeZoneAccuracyInvert = 60 - TimeZoneAccuracy;
  joystickX = analogRead(pinAxisX);
  joystickY = analogRead(pinAxisY);
  joystickZ = digitalRead(pinAxisZ) ^ 1;

  controllerBacklight() // TODO: test it

  releController = getReleControllerValue();

  if (!RTC.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
  }

  RtcDateTime now = RTC.GetDateTime();

  LCD.setCursor(0, 0);
  LCD.print(getDateTime(now));
  LCD.setCursor(18, 0);
  switch (now.DayOfWeek()) {
    case 0: LCD.print("BC"); break;
    case 1: LCD.print("nH"); break;
    case 2: LCD.print("BT"); break;
    case 3: LCD.print("CP"); break;
    case 4: LCD.print("4T"); break;
    case 5: LCD.print("nT"); break;
    case 6: LCD.print("C6"); break;
  }
  // Serial.println(getDateTime(now)); // TODO: only for test

  temp = DS18B20(tempAddr);

  LCD.setCursor(6, 3);
  LCD.print(RTC.GetMemory(accuracyPlace));
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

  delay(100);

  joystickControl();

  delay(100);
}

/*
 *
 * FUNCTIONS
 *
 */

#define countof(a) (sizeof(a) / sizeof(a[0]))

String getDateTime(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(
    datestring,
    countof(datestring),
    PSTR("%02u/%02u/%02u %02u:%02u:%02u"),
    dt.Year()-2000,
    dt.Month(),
    dt.Day(),
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

String getReleControllerValue() {
  LCD.setCursor(15, 3);

  if (!digitalRead(tumblerOn)) {
    LCD.print("*");
    return "on";
  }
  if (!digitalRead(tumblerOff)) {
    LCD.print("*");
    return "off";
  }

  LCD.print(" ");
  return "auto";
}

void doControlLed(String mode, bool red, bool yellow, bool green) {
  LCD.setCursor(0, 3);
  LCD.print(mode);
  digitalWrite(pinLedRed, red);
  digitalWrite(pinLedYellow, yellow);
  digitalWrite(pinLedGreen, green);
}

void doControl(int mode) {

  LCD.setCursor(17, 3);
  if (releController == "on") {
    LCD.print(" ON");
    digitalWrite(pinRelay, LOW);
  } else if (releController == "off") {
    LCD.print("OFF");
    digitalWrite(pinRelay, HIGH);
  }

  switch (mode) {
    case 1:
      doControlLed("$$$", true, false, false);

      if (releController == "auto") {
        LCD.setCursor(17, 3);
        LCD.print("OFF");
        digitalWrite(pinRelay, HIGH);
      }
      break;

    case 2:
      doControlLed("$$ ", false, true, false);

      if (releController == "auto") {
        LCD.setCursor(17, 3);

        if (temp < 40) {
          LCD.print(" ON");
          digitalWrite(pinRelay, LOW);
        } else {
          LCD.print("OFF");
          digitalWrite(pinRelay, HIGH);
        }
      }
      break;

    default:
      doControlLed("$  ", false, false, true);

      if (releController == "auto") {
        LCD.setCursor(17, 3);

        if (temp < 80) {
          LCD.print(" ON");
          digitalWrite(pinRelay, LOW);
        } else {
          LCD.print("OFF");
          digitalWrite(pinRelay, HIGH);
        }
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


void joystickControl() {
  int pressDelay = 0;

  if (editTimeMode) {
    if (editTimeMode == 7) {
      LCD.setCursor(6, 3);
    } else {
      LCD.setCursor(15 - ((editTimeMode - 1) * 3), 0);
    }
    LCD.print("  ");

    if (joystickY >= 600) {
      RtcDateTime now = RTC.GetDateTime();

      switch (editTimeMode) {
        case 1: RTC.SetDateTime(now + 1); break;
        case 2: RTC.SetDateTime(now + 60); break;
        case 3: RTC.SetDateTime(now + 3600); break;
        case 4: RTC.SetDateTime(now + 86400); break;
        case 5: RTC.SetDateTime(now + 2592000); break;
        case 6: RTC.SetDateTime(now + 31104000); break;
        case 7:
          int accuracyOld = RTC.GetMemory(accuracyPlace);
          int accuracyNew = accuracyOld == 15 ? 15 : accuracyOld + 1;
          RTC.SetMemory(0, accuracyNew);
          LCD.setCursor(6, 3);
          LCD.print(accuracyNew);
          break;
      }
      RTC.SetIsRunning(false);
      RTC.SetIsRunning(true);

    }
    if (joystickY <= 400) {
      RtcDateTime now = RTC.GetDateTime();

      switch (editTimeMode) {
        case 1: RTC.SetDateTime(now - 1); break;
        case 2: RTC.SetDateTime(now - 60); break;
        case 3: RTC.SetDateTime(now - 3600); break;
        case 4: RTC.SetDateTime(now - 86400); break;
        case 5: RTC.SetDateTime(now - 2592000); break;
        case 6: RTC.SetDateTime(now - 31104000); break;
        case 7:
          int accuracyOld = RTC.GetMemory(accuracyPlace);
          int accuracyNew = accuracyOld == 0 ? 0 : accuracyOld - 1;
          RTC.SetMemory(0, accuracyNew);
          LCD.setCursor(6, 3);
          LCD.print(accuracyNew);
          break;
      }
      RTC.SetIsRunning(false);
      RTC.SetIsRunning(true);
    }

    if (joystickX >= 600) {
      editTimeMode = editTimeMode == 1 ? 7 : editTimeMode - 1;
    }
    if (joystickX <= 400) {
      editTimeMode = editTimeMode == 7 ? 1 : editTimeMode + 1;
    }

    if (joystickZ) {
      editTimeMode = 0;
    }
  } else {
    if (joystickZ) {
      while(digitalRead(pinAxisZ) ^ 1) {
        pressDelay++;
      }
      if (pressDelay > 10) {
        editTimeMode = 1;
      }
    }
  }
}

void controllerBacklight () {
  if (joystickZ ||
      joystickX <= 400 || joystickX >= 600 ||
      joystickY <= 400 || joystickY >= 600
    ) {
      backlightTimeout = 15;
    }

  if (backlightTimeout > 0) {
    if (!backlight) {
      LCD.backlight();
      backlight = true
    }
    backlightTimeout = backlightTimeout - 0.2;
  } else {
    LCD.noBacklight();
    backlight = false
  }
}
