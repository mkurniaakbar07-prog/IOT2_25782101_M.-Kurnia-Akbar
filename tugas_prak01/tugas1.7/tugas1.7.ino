/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://docs.arduino.cc/hardware/

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/basics/Blink/
*/

// the setup function runs once when you press reset or power the board
const int buttonPin = 4;
const int ledPin = 5;

int ledState = LOW;
int lastButtonState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, ledState);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Mendeteksi saat tombol baru ditekan
  if (buttonState == HIGH && lastButtonState == LOW) {
    ledState = !ledState;

    digitalWrite(ledPin, ledState);

    if (ledState == HIGH) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    delay(50); // debounce sederhana
  }

  lastButtonState = buttonState;
}