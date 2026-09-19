// max31865.h
//
// 2x MAX31865 (SPI) para sondas PT1000, brazo exterior y tubo
// (REQUISITOS.md §4.3, §5). Un solo archivo, dos instancias: mismo
// sensor, mismo cableado (COMETA_MAX31865_WIRING, MAX31865_3WIRE),
// mismo RREF/RNOMINAL, CS distinto por sonda
// (COMETA_MAX31865_CS_ARM_PIN / _CS_TUBO_PIN). Atención: RREF=4300.0,
// RNOMINAL=1000.0 (PT1000, no PT100 como en los ejemplos de Adafruit).
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2), una vez
// por instancia.

#ifndef COMETA_SENSOR_MAX31865_H
#define COMETA_SENSOR_MAX31865_H

#include <Arduino.h>

#include "log_format.h"

// Brazo exterior: t_arm_C, err_arm.
namespace SensorPT1000Brazo {

bool iniciar();
bool actualizar(uint32_t ahora);
void llenarFila(FilaSCI &f);

}  // namespace SensorPT1000Brazo

// Tubo: t_tubo_C, err_tubo.
namespace SensorPT1000Tubo {

bool iniciar();
bool actualizar(uint32_t ahora);
void llenarFila(FilaSCI &f);

}  // namespace SensorPT1000Tubo

#endif  // COMETA_SENSOR_MAX31865_H
