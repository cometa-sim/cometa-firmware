// ms8607.h
//
// MS8607 (presión/temperatura/humedad, I²C 0x76 + 0x40, fondo de escala
// 10 hPa). REQUISITOS.md §4.3, §5.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).

#ifndef COMETA_SENSOR_MS8607_H
#define COMETA_SENSOR_MS8607_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorMS8607 {

bool iniciar();
bool actualizar(uint32_t ahora);

// Vuelca p_hPa, t_ms8607_C, rh_ms8607, solo si hay un dato nuevo.
void llenarFila(FilaSCI &f);

}  // namespace SensorMS8607

#endif  // COMETA_SENSOR_MS8607_H
