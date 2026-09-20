// geiger.h
//
// Geiger GGreg20, interrupción vía optoacoplador en COMETA_GEIGER_PIN
// (REQUISITOS.md §4.3, §5). La ISR solo incrementa un contador; los
// conteos se guardan por intervalo (cpi, dt_ms), sirven para el factor
// de Fano.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).

#ifndef COMETA_SENSOR_GEIGER_H
#define COMETA_SENSOR_GEIGER_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorGeiger {

// Configura el pin y attachInterrupt(); la ISR solo incrementa el
// contador (REQUISITOS.md §5).
bool iniciar();

bool actualizar(uint32_t ahora);

// Vuelca cpi (conteos del intervalo) y dt_ms (duración real del
// intervalo) en f, solo si hay un intervalo nuevo cerrado.
void llenarFila(FilaSCI &f);

}  // namespace SensorGeiger

#endif  // COMETA_SENSOR_GEIGER_H
