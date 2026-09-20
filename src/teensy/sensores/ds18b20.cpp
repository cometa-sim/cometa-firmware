// ds18b20.cpp
//
// Impalcatura: cuerpos vacíos, sin llamadas a OneWire/DallasTemperature.
// TODO implementar (REQUISITOS.md §4.3, §5):
//   - iniciar(): begin() en COMETA_DS18B20_BUS_PIN; verificar que las
//     cuatro ROM de config_teensy.h (COMETA_DS18B20_ROM_*) están en el
//     bus.
//   - actualizar(): ESTE es el caso de "arranque + recogida" del
//     presupuesto de bloqueo (REQUISITOS.md §1.2). La conversión a 12
//     bits tarda 750 ms, muchísimo más que COMETA_BLOQUEO_MAX_MS, así
//     que no se puede esperar dentro de la llamada. Con
//     setWaitForConversion(false), una pasada de actualizar() lanza
//     requestTemperatures() y se va; las siguientes miran si ya pasó el
//     tiempo de conversión y recién ahí leen. O sea: el sensor entrega
//     un dato nuevo cada dos tics de 1000 ms, no cada uno, y las filas
//     del medio llevan la celda vacía (§4.2) — que es exactamente lo
//     que hay que hacer, no repetir el último valor.
//     setResolution(COMETA_DS18B20_RESOLUCION_BITS) va en iniciar().
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
