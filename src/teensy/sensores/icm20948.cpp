// icm20948.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del ICM-20948.
// TODO implementar (REQUISITOS.md §4.3, §4.6, §5, §7):
//   - iniciar(): begin() a COMETA_ICM20948_I2C_ADDR, ±COMETA_ICM20948_RANGO_G,
//     COMETA_IMU_TASA_HZ.
//   - actualizar(): vaciar la FIFO; cada muestra alimenta el buffer
//     circular de main.cpp (actualizarBufferIMU()) y queda como "última
//     muestra" para llenarFila().
//   - llenarFila(): copiar la última muestra (ax..mz) a f, solo si es
//     nueva.

#include "icm20948.h"

#include "config_teensy.h"

namespace SensorICM20948 {

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

}  // namespace SensorICM20948
