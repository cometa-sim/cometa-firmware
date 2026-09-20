// ms8607.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del MS8607.
// TODO implementar (REQUISITOS.md §4.3, §5):
//   - iniciar(): begin() a COMETA_MS8607_I2C_ADDR_PRESION_TEMP /
//     _HUMEDAD.
//   - actualizar(): leer p_hPa, t_ms8607_C, rh_ms8607.
//   - llenarFila(): copiar el último dato a f, solo si es nuevo.

#include "ms8607.h"

#include "config_teensy.h"

namespace SensorMS8607 {

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

}  // namespace SensorMS8607
