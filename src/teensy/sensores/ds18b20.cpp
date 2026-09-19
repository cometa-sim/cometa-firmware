// ds18b20.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a OneWire/DallasTemperature.
// TODO implementar (REQUISITOS.md §4.3, §5):
//   - iniciar(): begin() en COMETA_DS18B20_BUS_PIN; verificar que las
//     cuatro ROM de config_teensy.h (COMETA_DS18B20_ROM_*) están en el
//     bus.
//   - actualizar(): setResolution(COMETA_DS18B20_RESOLUCION_BITS),
//     requestTemperatures() con setWaitForConversion(false): nunca
//     bloquear el loop 750 ms, recién leer cuando pasó el tiempo de
//     conversión.
//   - llenarFila(): copiar t_cassa_C, t_pile_C, t_centro_C, t_scd_ds_C a
//     f, cada una solo si es un dato nuevo.

#include "ds18b20.h"

#include "config_teensy.h"

namespace SensorDS18B20 {

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

}  // namespace SensorDS18B20
