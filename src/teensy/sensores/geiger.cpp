// geiger.cpp
//
// El contador volatile y la ISR están implementados de verdad: son la
// única parte del hardware que no puede esperar (si no hay ISR
// conectada, se pierden pulsos desde el primer instante). El resto
// (iniciar/actualizar/llenarFila) es impalcatura: cuerpos vacíos.
//
// TODO implementar (REQUISITOS.md §4.3, §5):
//   - iniciar(): pinMode(COMETA_GEIGER_PIN, ...) y
//     attachInterrupt(digitalPinToInterrupt(COMETA_GEIGER_PIN), isr, ...).
//   - actualizar()/llenarFila(): la lectura y el azaramiento del
//     contador van en un único bloque con interrupciones deshabilitadas,
//     para no perder ni duplicar un pulso que llegue justo en el medio:
//       noInterrupts();
//       uint32_t cuenta = contadorPulsos;
//       contadorPulsos = 0;
//       interrupts();
//     cpi = cuenta; dt_ms = milisegundos reales desde la última lectura
//     (no siempre exactamente 1000, por eso se guarda aparte).

#include "geiger.h"

#include "config_teensy.h"

namespace SensorGeiger {

namespace {

// La ISR solo incrementa un contador (REQUISITOS.md §5): nada de
// trabajo pesado ahí adentro. Todavía no está conectada con
// attachInterrupt() (ver TODO de iniciar() más abajo): el atributo
// evita el warning de función sin usar mientras tanto.
volatile uint32_t contadorPulsos = 0;

__attribute__((unused)) void isrPulso() {
  contadorPulsos++;
}

}  // namespace

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

}  // namespace SensorGeiger
