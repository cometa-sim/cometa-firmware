// sht45.h
//
// SHT45 (humedad/temperatura, I²C 0x44, en el brazo exterior).
// REQUISITOS.md §4.3, §5.
//
// Módulo de ejemplo (REQUISITOS.md §1.2): es el único sensor
// completamente implementado, para que el resto de los módulos se
// escriban copiando esta forma. Interfaz común: iniciar(), actualizar(),
// llenarFila().

#ifndef COMETA_SENSOR_SHT45_H
#define COMETA_SENSOR_SHT45_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorSHT45 {

// Inicializa el sensor en el bus I²C por defecto (Wire, REQUISITOS.md
// §5: SDA pin 18, SCL pin 19). Devuelve true si el SHT45 respondió.
bool iniciar();

// Lee humedad y temperatura. No bloquea el loop más que lo que tarda la
// propia conversión I²C del sensor (unos pocos ms en alta precisión):
// se llama desde el tic de 1 s, no del tic de 10 ms de la IMU. Devuelve
// true si la lectura fue válida (REQUISITOS.md §1.3: 5 fallos
// consecutivos marcan el sensor ausente).
bool actualizar(uint32_t ahora);

// Vuelca rh_sht y t_sht_C en f, solo si actualizar() trajo un dato
// nuevo desde la última llamada.
void llenarFila(FilaSCI &f);

}  // namespace SensorSHT45

#endif  // COMETA_SENSOR_SHT45_H
