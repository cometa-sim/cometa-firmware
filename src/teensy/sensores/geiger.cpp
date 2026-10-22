// geiger.cpp
//
// CÓDIGO DE ARQUITECTURA — NO TOCAR (REQUISITOS.md §1.2): el contador
// volatile y la ISR de abajo no son "el sensor", son la decisión de
// diseño de cómo se cuentan los pulsos, y están implementados de verdad.
// Si no hay ISR conectada se pierden pulsos desde el primer instante, y
// si el contador deja de ser volatile el compilador puede cachearlo y
// las cuentas salen mal de una forma que no se ve hasta analizar el
// vuelo.
//
// El resto (iniciar/actualizar/llenarFila) está implementado a partir
// del banco de prueba de los estudiantes (COMETA-Teensy41, main.cpp
// hasta 6ba266c): pin 3 con pull-up interno, interrupción en el flanco
// de bajada (la salida OUT del GGreg20 baja a GND en cada pulso) y la
// lectura del contador con las interrupciones deshabilitadas.
//
// Cambio respecto del banco de prueba, para cumplir REQUISITOS.md §4.3
// y §5: el contador se lee y se pone a cero en CADA tic de 1 s y se
// guarda por intervalo (cpi = conteos, dt_ms = duración real), no como
// CPM sobre 60 s. Con los conteos por intervalo se calculan después los
// CPM y también el factor de Fano, que con un único total por minuto no
// se puede calcular.
//
// Este módulo no pasa por la máquina de presencia/ausencia
// (REQUISITOS.md §1.3): main.cpp lo llama directo.

#include "geiger.h"

#include "config_teensy.h"

namespace SensorGeiger {

namespace {

// La ISR solo incrementa un contador (REQUISITOS.md §5): nada de
// trabajo pesado ahí adentro. Se conecta en iniciar().
volatile uint32_t contadorPulsos = 0;

void isrPulso() {
  contadorPulsos++;
}

// Intervalo cerrado en la última pasada de actualizar().
uint32_t cuentaIntervalo = 0;
uint32_t duracionIntervaloMs = 0;
uint32_t inicioIntervaloMs = 0;

}  // namespace

bool iniciar() {
  pinMode(COMETA_GEIGER_PIN, INPUT_PULLUP);
  inicioIntervaloMs = millis();
  attachInterrupt(digitalPinToInterrupt(COMETA_GEIGER_PIN), isrPulso, FALLING);
  return true;
}

bool actualizar(uint32_t ahora) {
  // Lectura y puesta a cero en un solo bloque, para no perder ni
  // duplicar un pulso que llegue justo en el medio (REQUISITOS.md §5).
  noInterrupts();
  const uint32_t cuenta = contadorPulsos;
  contadorPulsos = 0;
  interrupts();

  cuentaIntervalo = cuenta;
  duracionIntervaloMs = ahora - inicioIntervaloMs;  // 0 = celda vacía (§4.2)
  inicioIntervaloMs = ahora;
  return true;  // un pin con interrupción no tiene cómo fallar (§1.3)
}

void llenarFila(FilaSCI &f) {
  if (duracionIntervaloMs == 0) {
    return;  // intervalo vacío: celda vacía (REQUISITOS.md §4.2)
  }
  f.cpi = cuentaIntervalo;
  f.dt_ms = duracionIntervaloMs;
}

}  // namespace SensorGeiger
