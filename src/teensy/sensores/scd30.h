// scd30.h
//
// SCD30 (CO2, I²C 0x61, clock stretching largo). Sonda de referencia:
// REQUISITOS.md §3.4, §3.5, §4.3, §5.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2): un
// espacio de nombres por sensor, tres funciones. El manejo de "sensor
// ausente" (reintentos, umbral de fallos) vive en main.cpp, no acá
// (REQUISITOS.md §1.3).

#ifndef COMETA_SENSOR_SCD30_H
#define COMETA_SENSOR_SCD30_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorSCD30 {

// Intenta inicializar el sensor. true si responde.
bool iniciar();

// Lectura no bloqueante de esta pasada. true si la lectura fue válida
// (REQUISITOS.md §1.3: 5 fallos consecutivos marcan el sensor ausente).
bool actualizar(uint32_t ahora);

// Vuelca en f solo los campos propios (co2_ppm, t_scd_C, p_inviata_hPa),
// solo si hay un dato nuevo desde la última llamada.
void llenarFila(FilaSCI &f);

}  // namespace SensorSCD30

#endif  // COMETA_SENSOR_SCD30_H
