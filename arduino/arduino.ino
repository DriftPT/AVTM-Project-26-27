#include "Arduino_SensorKit.h"
#include <math.h>

// ------------------------------------------------------------
// SENSOR PINS

#define LIGHT A3
#define POT A0
#define LED 6
#define BUT 4


bool button_state;
int pot_value;
int light_value;

// Limites da chave com histerese
const int IGNITION_ON = 650;
const int IGNITION_OFF = 450;

bool engineOn = false;

void setup() {
  Serial.begin(9600);

  pinMode(LIGHT, INPUT);
  pinMode(POT, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUT, INPUT);

  Oled.begin();
  Oled.setFlipMode(true);
  Oled.setFont(u8x8_font_amstrad_cpc_extended_r);

  Accelerometer.begin();

  // IGNITION
  int pot_value = analogRead(POT);
  
  if (!engineOn && pot_value >= IGNITION_ON) {
    engineOn = true;
    digitalWrite(LED, HIGH);
  }
  else if (engineOn && pot_value <= IGNITION_OFF) {
    engineOn = false;
    digitalWrite(LED, LOW);
  }

  Oled.setCursor(0, 6);
  Oled.print("Engine:");
  Oled.print(engineOn ? "ON " : "OFF");

}

void loop() {

  // ------------------------------------------------------------
  // ACCELEROMETER
  Oled.setCursor(0, 4);

  float x = Accelerometer.readX();
  float y = Accelerometer.readY();
  float z = Accelerometer.readZ();

  // Inclinação frente/trás
  float pitch = atan2(x, sqrt(y * y + z * z)) * 180.0 / PI;

  // Inclinação esquerda/direita
  float roll = atan2(y, sqrt(x * x + z * z)) * 180.0 / PI;

  Oled.print("pitch:");
  Oled.print(pitch);
  Oled.setCursor(0, 5);
  Oled.print("roll:");
  Oled.print(roll);


  // ------------------------------------------------------------
  // LIGHT SENSOR
  light_value = analogRead(LIGHT);
  Oled.setCursor(0, 3);
  Oled.print("Light: ");
  Oled.print(light_value);
  Oled.print("   ");

  // ------------------------------------------------------------
  // IGNITION
  pot_value = analogRead(POT);

  if (!engineOn && pot_value >= IGNITION_ON) {
    engineOn = true;
    digitalWrite(LED, HIGH);
  }
  else if (engineOn && pot_value <= IGNITION_OFF) {
    engineOn = false;
    digitalWrite(LED, LOW);
  }

  Oled.setCursor(0, 6);
  Oled.print("Engine:");
  Oled.print(engineOn ? "ON " : "OFF");


  // ------------------------------------------------------------
  // BOOST BUTTON
  button_state = digitalRead(BUT);

  Oled.setCursor(0, 7);
  Oled.print("Boost: ");
  Oled.print(button_state ? "ON " : "OFF");



  // ------------------------------------------------------------
  // SERIAL COMMUNICATION
  Serial.print(pitch, 1);
  Serial.print(",");

  Serial.print(roll, 1);
  Serial.print(",");

  Serial.print(light_value);
  Serial.print(",");

  Serial.print(engineOn ? 1 : 0);
  Serial.print(",");

  Serial.println(button_state ? 1 : 0);

  delay(30);
}