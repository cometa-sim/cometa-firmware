// ds18b20.h
//
// 4x DS18B20 (1-Wire, un bus, pull-up 4.7 kΩ), identificadas por ROM:
// caja, pilas, centro del volumen, SCD30 (REQUISITOS.md §4.3, §5).
// Resolución 12 bits con setWaitForConversion(false): nunca bloquear el
// loop 750 ms. Un solo módulo para las cuatro sondas (comparten bus).
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).

#ifndef COMETA_SENSOR_DS18B20_H
#define COMETA_SENSOR_DS18B20_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorDS18B20 {

bool iniciar();
bool actualizar(uint32_t ahora);

// Vuelca t_cassa_C, t_pile_C, t_centro_C, t_scd_ds_C, solo los que
// tengan un dato nuevo.
void llenarFila(FilaSCI &f);

}  // namespace SensorDS18B20

#endif  // COMETA_SENSOR_DS18B20_H
