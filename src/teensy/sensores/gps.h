// gps.h
//
// SAM-M8Q propio del Teensy, I²C 0x42 (REQUISITOS.md §3.1, §3.9, §4.3,
// §5). Generación M8: usar los métodos legacy (CFG-NAV5), no
// setVal/VALSET.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).

#ifndef COMETA_SENSOR_GPS_H
#define COMETA_SENSOR_GPS_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorGPS {

// Envía DYN_MODEL_AIRBORNE1g (CFG-NAV5 legacy) y relee con
// getDynamicModel() para confirmar (REQUISITOS.md §3.1). El resultado
// de esa relectura va a META (fuera de este módulo, ver main.cpp).
bool iniciar();

bool actualizar(uint32_t ahora);

// Vuelca lat, lon, alt_m, vz_ms, vn_ms, ve_ms, sats, pdop, fix, utc,
// solo si hay un fix nuevo.
void llenarFila(FilaSCI &f);

}  // namespace SensorGPS

#endif  // COMETA_SENSOR_GPS_H
