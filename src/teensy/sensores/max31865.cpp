// max31865.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del MAX31865.
// TODO implementar en cada instancia (REQUISITOS.md §4.3, §5):
//   - iniciar(): begin(COMETA_MAX31865_WIRING) con el CS propio
//     (COMETA_MAX31865_CS_ARM_PIN / _CS_TUBO_PIN).
//   - actualizar(): leer temperatura con RREF/RNOMINAL de
//     config_teensy.h y el fault byte.
//   - llenarFila(): copiar t_arm_C/err_arm (brazo) o t_tubo_C/err_tubo
//     (tubo) a f, solo si hay un dato nuevo.

#include "max31865.h"

#include "config_teensy.h"

namespace SensorPT1000Brazo {

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

}  // namespace SensorPT1000Brazo

namespace SensorPT1000Tubo {

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

}  // namespace SensorPT1000Tubo
