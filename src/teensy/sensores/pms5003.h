// pms5003.h
//
// PMS5003 por UART Serial1 (RX pin 0, TX pin 1), controlado por MOSFET
// de canal N que corta la masa del sensor (REQUISITOS.md §3.6, §3.7,
// §4.3, §6).
//
// Interfaz común de los módulos de sensor (REQUISITOS.md §1.2), más dos
// funciones propias de este sensor (apagar/encender el MOSFET y la
// secuencia de Serial1) y el control de encendido por altitud/
// temperatura (REQUISITOS.md §6), que no son parte de la interfaz común
// porque ningún otro sensor las necesita.

#ifndef COMETA_SENSOR_PMS5003_H
#define COMETA_SENSOR_PMS5003_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorPMS5003 {

bool iniciar();
bool actualizar(uint32_t ahora);

// Vuelca pms_on, pm1, pm25, pm10, n03, n05, n10 a f. pms_on se registra
// siempre; el resto solo si hay un dato nuevo (se descartan las
// primeras COMETA_PMS_CALENTAMIENTO_S de cada encendido, REQUISITOS.md
// §6).
void llenarFila(FilaSCI &f);

// Apaga el PMS5003 de verdad: Serial1 en alta impedancia y recién
// después el MOSFET a LOW (REQUISITOS.md §6).
void apagar();

// Enciende el PMS5003: MOSFET a HIGH y recién después Serial1
// (REQUISITOS.md §6).
void encender();

// Decide si el PMS debe estar encendido según altitud y temperatura
// exterior, y llama a encender()/apagar() solo en los cambios de estado
// (REQUISITOS.md §6).
void controlarEncendido(uint32_t ahora, float altitud_m, float temperatura_C);

}  // namespace SensorPMS5003

#endif  // COMETA_SENSOR_PMS5003_H
