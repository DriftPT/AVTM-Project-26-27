#include "Arduino_SensorKit.h"
#include <math.h>

// ------------------------------------------------------------
// SENSOR PINS

#define LIGHT A2
#define POT A1
#define LED A0
#define BUT 6


bool button_state;
int pot_value;
int light_value; 

const int IGNITION_MOVEMENT = 100;

bool engineOn = false;
int ignitionReference = 0;

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
  pot_value = analogRead(POT);
  ignitionReference = pot_value;

  engineOn = false;
  digitalWrite(LED, LOW);

  Oled.setCursor(0, 6);
  Oled.print("Engine:OFF");

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
  // IGNITION - Relative rotation

  pot_value = analogRead(POT);

  if (!engineOn) {

    // Se rodarmos no sentido contrário, atualizamos a referência
    if (pot_value < ignitionReference) {
      ignitionReference = pot_value;
    }

    // Rodar 100 unidades para a direita liga
    if (pot_value - ignitionReference >= IGNITION_MOVEMENT) {
      engineOn = true;
      ignitionReference = pot_value;
    }

  }
  else {

    // Se rodarmos no sentido contrário, atualizamos a referência
    if (pot_value > ignitionReference) {
      ignitionReference = pot_value;
    }

    // Rodar 100 unidades para a esquerda desliga
    if (ignitionReference - pot_value >= IGNITION_MOVEMENT) {
      engineOn = false;
      ignitionReference = pot_value;
    }

  }

  digitalWrite(LED, engineOn ? HIGH : LOW);

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