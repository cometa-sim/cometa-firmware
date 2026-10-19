// COMETA · Feather M0 Adalogger — prueba de pines analógicos (NO VUELA)
//
// El Feather como voltímetro, para revisar el divisor de batería sin
// multímetro (REQUISITOS.md §2.1): cada segundo escribe por Serial la
// tensión en A0-A5 y A7, promediada como v_batt en el firmware de vuelo.
//
// Uso: con el USB conectado y el pack de 3 pilas también (con 3 pilas el
// pack, después del diodo, queda por debajo de los 5 V del USB y no le
// manda corriente a la PC), mirar el monitor mientras se prende y se
// apaga el interruptor del pack. El pin del divisor pasa de la mitad del
// pack (~2,35 V con 3 alcalinas) a 0 V; un pin sin nada conectado da
// valores que saltan y no siguen al interruptor. Debería ser A2
// (COMETA_VBATT_ADC_PIN).
//
//   pio run -e adalogger_pines -t upload && pio device monitor -e adalogger_pines
//
// Después se vuelve a cargar el entorno adalogger.

#include <Arduino.h>

#include "config_adalogger.h"

struct PinAnalogico {
  const char *nombre;
  uint8_t pin;
};

const PinAnalogico PINES[] = {
    {"A0", A0}, {"A1", A1}, {"A2", A2}, {"A3", A3},
    {"A4", A4}, {"A5", A5}, {"A7", A7},  // A7: divisor de fábrica del JST BAT
};

float leerVoltios(uint8_t pin) {
  uint32_t suma = 0;
  for (int i = 0; i < COMETA_VBATT_MUESTRAS; i++) {
    suma += analogRead(pin);
  }
  return (float)suma / COMETA_VBATT_MUESTRAS *
         (COMETA_ADC_VREF / ((1 << COMETA_ADC_BITS) - 1));
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(COMETA_ADC_BITS);
}

void loop() {
  static uint32_t ultimo = 0;
  const uint32_t ahora = millis();
  if (ahora - ultimo < COMETA_TICK_L2_MS) {
    return;
  }
  ultimo = ahora;

  for (const PinAnalogico &p : PINES) {
    Serial.print(p.nombre);
    Serial.print('=');
    Serial.print(leerVoltios(p.pin), 2);
    Serial.print(" V  ");
  }
  // Lo que el firmware de vuelo escribiría como v_batt.
  Serial.print("| v_batt=");
  Serial.println(leerVoltios(COMETA_VBATT_ADC_PIN) * COMETA_VBATT_DIVISOR_FACTOR, 2);
}
