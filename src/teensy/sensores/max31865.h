// max31865.h
//
// 2x MAX31865 (SPI) para sondas PT1000, A y B (REQUISITOS.md §4.3, §5).
// Un solo archivo, dos instancias: mismo sensor, mismo cableado
// (COMETA_MAX31865_WIRING, MAX31865_3WIRE), mismo RREF/RNOMINAL, bus y
// CS distintos por sonda (A en SPI con COMETA_MAX31865_CS_A_PIN, B en
// SPI1 con COMETA_MAX31865_CS_B_PIN). Atención: RREF=4300.0,
// RNOMINAL=1000.0 (PT1000, no PT100 como en los ejemplos de Adafruit).
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2), una vez
// por instancia.

#ifndef COMETA_SENSOR_MAX31865_H
#define COMETA_SENSOR_MAX31865_H

#include <Arduino.h>

#include "log_format.h"

// A (la sonda exterior): t_ptA_C, err_ptA.
namespace SensorPT1000A {

bool iniciar();
bool actualizar(uint32_t ahora);
void llenarFila(FilaSCI &f);

}  // namespace SensorPT1000A

// B: t_ptB_C, err_ptB.
namespace SensorPT1000B {

bool iniciar();
bool actualizar(uint32_t ahora);
void llenarFila(FilaSCI &f);

}  // namespace SensorPT1000B

#endif  // COMETA_SENSOR_MAX31865_H
