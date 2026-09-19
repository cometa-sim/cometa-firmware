// icm20948.h
//
// ICM-20948 (IMU, I²C 0x69, breakout con AD0 alto; ±16 g). REQUISITOS.md
// §4.3, §4.6, §5, §7.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).
// actualizar() se llama en el tic de COMETA_TICK_IMU_MS (100 Hz): vacía
// la FIFO interna del ICM-20948 en cada pasada, para no perder muestras
// si el tic de 1000 ms se retrasó (clock stretching del SCD30, GPS,
// flush de la SD). El buffer circular de la ventana IMU (REQUISITOS.md
// §7) es responsabilidad de main.cpp, no de este módulo: acá solo se
// mantiene la última muestra para llenarFila().

#ifndef COMETA_SENSOR_ICM20948_H
#define COMETA_SENSOR_ICM20948_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorICM20948 {

bool iniciar();

// Vacía la FIFO (100 Hz); cada muestra conserva su propio t_ms para el
// buffer circular de la ventana IMU (REQUISITOS.md §7).
bool actualizar(uint32_t ahora);

// Vuelca ax..mz (última muestra) en la fila SCI, solo si hay un dato
// nuevo.
void llenarFila(FilaSCI &f);

}  // namespace SensorICM20948

#endif  // COMETA_SENSOR_ICM20948_H
