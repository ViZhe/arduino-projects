
//
// # The controller of the boiler
//
//
// ## Connections:
// DS1307:  SDA       -> A4
//          SCL       -> A5
// LCD:     SDA       -> A4
//          SCL       -> A5

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RtcDS1307.h>


LiquidCrystal_I2C lcd(0x27, 20, 4);
RtcDS1307 Rtc;


void setup() {
    Serial.begin(9600);
    lcd.init();
    lcd.backlight();
    Rtc.Begin();

    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);
    // if you are using ESP-01 then uncomment the line below to reset the pins to
    // the available pins for SDA, SCL
    // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    Serial.print(printDateTime(compiled));

    if (!Rtc.IsDateTimeValid()) {

        Serial.println("RTC lost confidence in the DateTime!");

        // following line sets the RTC to the date & time this sketch was compiled
        // it will also reset the valid flag internally unless the Rtc device is
        // having an issue

        Rtc.SetDateTime(compiled);

    }

    if (!Rtc.GetIsRunning()) {

        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);

    }
    Rtc.SetIsRunning(true);

    RtcDateTime now = Rtc.GetDateTime();

    if (now < compiled) {

        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);

    } else if (now > compiled) {

        Serial.println("RTC is newer than compile time. (this is expected)");

    } else if (now == compiled) {

        Serial.println("RTC is the same as compile time! (not expected but all is fine)");

    }

    Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);
}

void loop() {
    lcd.setCursor(0, 0);
    lcd.print("TIME");

    if (!Rtc.IsDateTimeValid()) {
        Serial.println("RTC lost confidence in the DateTime!");
    }

    RtcDateTime now = Rtc.GetDateTime();

    lcd.setCursor(0, 1);
    lcd.print(printDateTime(now));
    Serial.println(printDateTime(now));

    delay (1000);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

String printDateTime(const RtcDateTime& dt) {
    char datestring[20];

    snprintf_P(
        datestring,
        countof(datestring),
        PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
        dt.Month(),
        dt.Day(),
        dt.Year(),
        dt.Hour(),
        dt.Minute(),
        dt.Second()
    );

    return datestring;
}
