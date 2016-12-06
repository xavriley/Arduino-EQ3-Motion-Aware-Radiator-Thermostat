/*
 * Motion aware thermostat (RoomTooCold)
 */
#include <SoftwareSerial.h>

SoftwareSerial bluetooth(2, 3);

int tempSensorPin = 0;
int inputPin = 4;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    // variable for reading the pin status

float targetTemperature = 20.0;

void logOutput() {
  while (bluetooth.available()) {
    Serial.write(bluetooth.read());
  }
  Serial.println("");
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);      // declare LED as output
  pinMode(inputPin, INPUT);     // declare sensor as input

  Serial.begin(9600);
  bluetooth.begin(9600);

  // wait for Serial to be ready
  while(!Serial){}

  bluetooth.print("AT+RENEW"); // should return OK
  delay(5000);

  bluetooth.print("AT+MARJ0x1000"); // set Major version to 0x1000
  delay(100);
  bluetooth.print("AT+MINO0x0101"); // set Minor version to 0x0101
  delay(100);
  bluetooth.print("AT+ADVI4"); // set advertising interval 5 seconds
  delay(100);
  bluetooth.print("AT+NAMETCHKR"); // set name to TCHKR
  delay(100);
  bluetooth.print("AT+IBEA1"); // enable iBeacon mode
  delay(100);
  bluetooth.print("AT+DELO2"); // broadcast only to save power
  delay(100);
  //mySerial.print("AT+PWRM1"); // additional power management if needed
  //delay(100);
  bluetooth.print("AT+RESET"); // Let above settings take effect
  delay(500);

  logOutput();
}

float getTemperature() {
   //getting the voltage reading from the temperature sensor
 int reading = analogRead(tempSensorPin);

 // converting that reading to voltage, for 3.3v arduino use 3.3
 float voltage = reading * 5.0;
 voltage /= 1024.0;

 float temperatureC = (voltage - 0.5) * 100 ;

 return temperatureC;
}

boolean motionDetected() {
  val = digitalRead(inputPin);  // read input value
  return val == HIGH;           // check if the input is HIGH
}

boolean roomTooCold() {
  return getTemperature() < targetTemperature;
}

void loop(){

  if (motionDetected() && roomTooCold()) {
    digitalWrite(LED_BUILTIN, HIGH);  // turn LED ON
    if (pirState == LOW) {
      // we have just turned on
      Serial.println("Motion detected!");

      // broadcast different minor version for BLE beacon
      // signals that room is too cold
      bluetooth.print("AT+MINO0x0102");
      logOutput();

      // We only want to print on the output change, not state
      pirState = HIGH;
    }
  } else {
    digitalWrite(LED_BUILTIN, LOW); // turn LED OFF
    if (pirState == HIGH){
      // we have just turned off
      Serial.println("Motion ended!");

      // broadcast original minor version for BLE beacon
      // signals that room is too warm
      bluetooth.print("AT+MINO0x0101");
      logOutput();

      // We only want to print on the output change, not state
      pirState = LOW;
    }
  }
}
