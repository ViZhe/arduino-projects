
#include <LiquidCrystal.h>

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

int pinLed = 13;
int pinTemp = A1;

void setup() {
    lcd.begin(16, 2);
    lcd.setCursor(1, 0);
    lcd.print("Hellow Hopa!");

    Serial.begin(9600);

    pinMode(pinLed, OUTPUT);
}


int tempC = 0;
void loop() {
    delay(1000);
    tempC = 5.0 * analogRead(pinTemp) / 10.24;

    Serial.println("Temperature: " + tempC);

    if (tempC > 50) {
        digitalWrite(pinLed, HIGH);
    } else {
        digitalWrite(pinLed, LOW);
        // digitalWrite(piezo, HIGH);
    }

    lcd.setCursor(0, 0);
    lcd.print("Temperature: ");
    lcd.setCursor(0, 1);
    lcd.print(tempC);
    lcd.print("C                ");
}
