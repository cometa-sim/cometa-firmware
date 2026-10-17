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
// §7) es responsabilidad de main.cpp, no de este módulo: acá se entregan
// las muestras nuevas (tomarMuestras()) y se guarda la última para
// llenarFila().

#ifndef COMETA_SENSOR_ICM20948_H
#define COMETA_SENSOR_ICM20948_H

#include <Arduino.h>

#include "log_format.h"

namespace SensorICM20948 {

// Una muestra tal como sale del chip, en binario (el buffer circular de
// la ventana IMU guarda esto, §7): 22 bytes en vez de los 40 de FilaIMU.
struct MuestraIMU {
  uint32_t t_ms;
  int16_t a[3];  // acelerómetro, cuentas a ±COMETA_ICM20948_RANGO_G
  int16_t g[3];  // giróscopo, cuentas a ±COMETA_ICM20948_RANGO_DPS
  int16_t m[3];  // magnetómetro, 0,15 µT por cuenta
};

bool iniciar();

// Vacía la FIFO (100 Hz); cada muestra conserva su propio t_ms para el
// buffer circular de la ventana IMU (REQUISITOS.md §7).
bool actualizar(uint32_t ahora);

// Vuelca ax..mz (última muestra) en la fila SCI, solo si hay un dato
// nuevo.
void llenarFila(FilaSCI &f);

// Las muestras leídas en el último actualizar(), de la más vieja a la
// más nueva. Cada una se entrega una sola vez: una segunda llamada sin
// actualizar() de por medio devuelve 0.
size_t tomarMuestras(const MuestraIMU **muestras);

// Muestra cruda -> unidades del log IMU (g, °/s, µT).
void aFilaIMU(const MuestraIMU &m, FilaIMU &f);

// |a| en g, para el criterio de caída libre (§7).
float moduloAceleracionG(const MuestraIMU &m);

// Contadores desde el arranque, para el banco y META: muestras leídas de
// la FIFO, y veces que hubo que reiniciarla por desalineada o llena.
uint32_t muestrasTotales();
uint32_t resincronizaciones();

}  // namespace SensorICM20948

#endif  // COMETA_SENSOR_ICM20948_H
