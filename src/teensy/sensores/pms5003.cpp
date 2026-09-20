// pms5003.cpp
//
// CÓDIGO DE ARQUITECTURA — NO TOCAR (REQUISITOS.md §1.2): apagar() y
// encender() están implementadas de verdad. No son "lógica de sensor":
// son la secuencia exacta de pines que evita la fuga de corriente por TX
// descrita abajo (REQUISITOS.md §6), y el orden de las líneas importa.
// iniciar()/actualizar()/llenarFila()/controlarEncendido() sí son
// impalcatura: cuerpos vacíos, sin llamadas a ninguna librería de
// parseo del PMS5003.
//
// TODO implementar (REQUISITOS.md §3.6, §3.7, §4.3, §6):
//   - iniciar(): configurar COMETA_PMS5003_MOSFET_PIN como OUTPUT y
//     llamar a apagar() para arrancar en un estado conocido.
//   - actualizar(): parsear la trama del PMS5003 por Serial1.
//   - llenarFila(): pm1, pm25, pm10, n03, n05, n10; descartar las
//     primeras COMETA_PMS_CALENTAMIENTO_S de cada encendido (el
//     ventilador tarda en estabilizarse). pms_on se registra siempre,
//     incluso si el sensor todavía no fue iniciar()-ado con éxito: sin
//     eso, en el análisis no se distingue un cero de un sensor apagado.
//   - controlarEncendido(): apagar por encima de 5 km; por debajo, en
//     subida, histéresis −15/−12 °C con T exterior (PT1000 brazo); en
//     bajada tras el estallido, reencender por debajo de 5 km.

#include "pms5003.h"

#include "config_teensy.h"

namespace SensorPMS5003 {

// Apaga el PMS5003: primero pone RX/TX de Serial1 en alta impedancia y
// recién después corta la masa por el MOSFET. El MOSFET (canal N) corta
// la masa, no la alimentación positiva: con la masa cortada, si TX
// quedara activo su reposo en alto inyectaría corriente (hasta 8 mA) por
// los diodos de protección del PMS y lo alimentaría a medias — parece
// apagado y no lo está, con riesgo de dejarlo en un estado indefinido, y
// en 4 h de vuelo son ~34 mAh (~4 % del balance de energía).
void apagar() {
  COMETA_PMS5003_SERIAL.end();
  pinMode(COMETA_PMS5003_RX_PIN, INPUT);  // alta impedancia: nada de corriente hacia el sensor
  pinMode(COMETA_PMS5003_TX_PIN, INPUT);
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, LOW);
}

// Enciende el PMS5003: primero el MOSFET, después Serial1 (con
// COMETA_PMS5003_MOSFET_SETTLE_MS de por medio para el asentamiento del
// regulador). Ese delay() bloquea el loop unos 50 ms: aceptable porque
// solo ocurre en los cambios de estado de controlarEncendido(), no en
// cada vuelta.
void encender() {
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, HIGH);
  delay(COMETA_PMS5003_MOSFET_SETTLE_MS);
  COMETA_PMS5003_SERIAL.begin(COMETA_PMS5003_BAUD);
}

bool iniciar() {
  // TODO
  return false;
}

bool actualizar(uint32_t ahora) {
  (void)ahora;
  // TODO
  return false;
}

void llenarFila(FilaSCI &f) {
  (void)f;
  // TODO
}

void controlarEncendido(uint32_t ahora, float altitud_m, float temperatura_C) {
  (void)ahora;
  (void)altitud_m;
  (void)temperatura_C;
  // TODO
}

}  // namespace SensorPMS5003
