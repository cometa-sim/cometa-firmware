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

// Presión de la última lectura válida, en hPa; NAN si la última pasada
// falló. La usa el SCD30 para setAmbientPressure() (REQUISITOS.md §3.4):
// como el SCD30 se actualiza antes que el MS8607 en el tic, recibe la
// presión del tic anterior (1 s de atraso, despreciable).
float ultimaPresionHPa();

}  // namespace SensorMS8607

#endif  // COMETA_SENSOR_MS8607_H
