// ltr390.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del LTR390.
// TODO implementar (REQUISITOS.md §4.3, §5):
//   - iniciar(): begin() a COMETA_LTR390_I2C_ADDR.
//   - actualizar(): leer uva_raw, uv_gain, uv_res (conteos crudos).
//   - llenarFila(): copiar el último dato a f, solo si es nuevo.

#include "ltr390.h"

#include "config_teensy.h"

namespace SensorLTR390 {

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

}  // namespace SensorLTR390
