// gps.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del u-blox.
// TODO implementar (REQUISITOS.md §3.1, §3.9, §4.3, §5):
//   - iniciar(): begin() a COMETA_GPS_I2C_ADDR; setDynamicModel a
//     DYN_MODEL_AIRBORNE1g (CFG-NAV5 legacy, no setVal/VALSET); releer
//     con getDynamicModel() y registrar el resultado en META (fuera de
//     este módulo).
//   - actualizar(): leer posición/velocidad/UTC cuando haya fix nuevo.
//   - llenarFila(): copiar lat, lon, alt_m, vz_ms, vn_ms, ve_ms, sats,
//     pdop, fix, utc a f, solo si hay un fix nuevo. utc en formato ISO
//     8601, vacío hasta tener fix (REQUISITOS.md §4.2).
//     ATENCIÓN con utc: se escribe SOLO con copiarUTC(f.utc, cadena) de
//     log_format.h, nunca con strcpy() ni sprintf() directo sobre f.utc.
//     Es un char[21] justo: una cadena más larga (un formato con
//     milisegundos, una trama mal parseada) pisaría lat y lon, que están
//     pegados en la struct, y el log saldría con posiciones absurdas sin
//     ningún error visible. copiarUTC() trunca en vez de desbordar.

#include "gps.h"

#include "config_teensy.h"

namespace SensorGPS {

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

}  // namespace SensorGPS
