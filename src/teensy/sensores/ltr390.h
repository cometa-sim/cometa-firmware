// ltr390.h
//
// LTR390 (UV, I²C 0x53, conteos crudos). REQUISITOS.md §4.3, §5.
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2).

#ifndef COMETA_SENSOR_LTR390_H
#define COMETA_SENSOR_LTR390_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorLTR390 {

bool iniciar();
bool actualizar(uint32_t ahora);

// Vuelca uva_raw, uv_gain, uv_res, solo si hay un dato nuevo.
void llenarFila(FilaSCI &f);

}  // namespace SensorLTR390

#endif  // COMETA_SENSOR_LTR390_H
