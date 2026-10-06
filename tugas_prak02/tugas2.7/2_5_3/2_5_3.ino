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
#include <DHT.h>

#define DHTPIN D7
#define DHTTYPE DHT22

const int ldrPin = A0;
const int relayPin = D6;
const int ledPin = D1;

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Kondisi awal: aktuator mati
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LOW);

  Serial.println("Sistem Monitoring Dimulai");
}

void loop() {
  float suhu = dht.readTemperature();
  int ldrValue = analogRead(ldrPin);

  // Cek apakah pembacaan DHT valid
  if (isnan(suhu)) {
    Serial.println("Gagal membaca DHT22!");
    delay(2000);
    return;
  }

  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.print(" C | LDR: ");
  Serial.println(ldrValue);

  // Suhu > 34 ATAU LDR < 300
  if (suhu > 40|| ldrValue < 500) {

    // Untuk relay Active-Low
    digitalWrite(relayPin, LOW);
    digitalWrite(ledPin, HIGH);

    Serial.println("Peringatan: Aktuator Aktif!");

  } else {

    // Relay mati
    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, LOW);

    Serial.println("Kondisi Aman");
  }

  Serial.println("-------------------------");

  delay(2000);
}