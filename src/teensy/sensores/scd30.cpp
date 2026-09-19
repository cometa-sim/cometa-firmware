// scd30.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a la librería del SCD30.
// TODO implementar (REQUISITOS.md §3.4, §3.5, §4.3, §5):
//   - iniciar(): begin() a COMETA_SCD30_I2C_ADDR.
//   - actualizar(): setAutoSelfCalibration(false) una vez al iniciar;
//     enviar la presión medida con setAmbientPressure() (rango
//     COMETA_SCD30_PRESION_MIN_MBAR..MAX_MBAR, fuera de rango se envía
//     el límite) y registrar el valor enviado en p_inviata_hPa; leer
//     co2_ppm y t_scd_C cuando haya dato nuevo (actualiza cada 2 s).
//   - llenarFila(): copiar el último dato a f, solo si es nuevo.

#include "scd30.h"

#include "config_teensy.h"

namespace SensorSCD30 {

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

}  // namespace SensorSCD30
