// geiger.cpp
//
// CÓDIGO DE ARQUITECTURA — NO TOCAR (REQUISITOS.md §1.2): el contador
// volatile y la ISR de abajo no son "el sensor", son la decisión de
// diseño de cómo se cuentan los pulsos, y están implementados de verdad.
// Si no hay ISR conectada se pierden pulsos desde el primer instante, y
// si el contador deja de ser volatile el compilador puede cachearlo y
// las cuentas salen mal de una forma que no se ve hasta analizar el
// vuelo. El resto (iniciar/actualizar/llenarFila) sí es impalcatura:
// cuerpos vacíos a completar.
//
// Este módulo no pasa por la máquina de presencia/ausencia
// (REQUISITOS.md §1.3): main.cpp lo llama directo.
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
