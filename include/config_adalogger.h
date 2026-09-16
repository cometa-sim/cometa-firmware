// config_adalogger.h
//
// Constantes de la cadena de respaldo (Feather M0 Adalogger, nivel 2).
// Ningún número mágico en el código: todo lo que aparece aquí está
// definido en REQUISITOS.md. Los valores marcados [VERIFICAR] no están
// confirmados: no se inventan, se miden o se consultan en la hoja de
// datos antes del vuelo.
//
// Esta placa es independiente de la del Teensy (REQUISITOS.md §2): no
// comparte bus, alimentación ni masa. La única relación entre los dos
// logs es el UTC del GPS.

#ifndef COMETA_CONFIG_ADALOGGER_H
#define COMETA_CONFIG_ADALOGGER_H

#include <Arduino.h>
#include <math.h>

// -----------------------------------------------------------------------
// Centinela para el único valor [VERIFICAR] que queda en esta placa: el
// timeout de recuperación I²C. Es un valor imposible en la práctica, para
// que el placeholder sea evidente en vez de un número creíble.
// -----------------------------------------------------------------------
#define COMETA_MS_VERIFICAR (-1L)  // TODO VERIFICAR: duración sin confirmar

// -----------------------------------------------------------------------
// §3.1 — GPS en modo Airborne <1g (SAM-M8Q propio, generación M8:
// CFG-NAV5 legacy, NO setVal/VALSET)
// -----------------------------------------------------------------------
#define COMETA_GPS_I2C_ADDR 0x42
// TODO VERIFICAR: ¿DYN_MODEL_AIRBORNE1g se guarda en flash o solo en BBR?
// (REQUISITOS.md §3.1). Se decide al implementar inicializarGPS(): no es
// un valor que se pueda fijar de antemano en una constante.

// -----------------------------------------------------------------------
// §3.2 — flush() periódico del log
// -----------------------------------------------------------------------
#define COMETA_FLUSH_CADA_N_MUESTRAS 10

// -----------------------------------------------------------------------
// §3.3 — Recuperación del bus I²C (9 pulsos en SCL si SDA queda baja)
// -----------------------------------------------------------------------
#define COMETA_I2C_RECOVERY_PULSOS_SCL 9
// TODO VERIFICAR: timeout de recuperación (REQUISITOS.md §3.3). A
// diferencia del Teensy, en este bus solo está el GPS (sin SCD30): no
// depende de la prueba 13, alcanza con medirlo en banco.
#define COMETA_I2C_TIMEOUT_MS COMETA_MS_VERIFICAR

// -----------------------------------------------------------------------
// §2 — Periodo de loop() no bloqueante (tick basado en millis(), sin
// delay()): un solo tick a 1 Hz para el log L2 (REQUISITOS.md §4.1).
// -----------------------------------------------------------------------
#define COMETA_TICK_L2_MS 1000UL

// -----------------------------------------------------------------------
// §2 — SD: chip select pin 4
// -----------------------------------------------------------------------
#define COMETA_SD_CS_PIN 4

// -----------------------------------------------------------------------
// §2.1, §3.10 — Tensión de batería por ADC. El pack se conecta por el pin
// USB a través de un diodo (1N5819/1N5817), nunca por el JST BAT: el pin
// A7 de fábrica del Feather M0 (que mide la batería del JST) NO sirve acá.
// Divisor propio 100 kΩ / 100 kΩ en A1 (REQUISITOS.md §2.1).
// -----------------------------------------------------------------------
#define COMETA_VBATT_ADC_PIN        A1
#define COMETA_VBATT_DIVISOR_FACTOR 2.0

// -----------------------------------------------------------------------
// §4.1 — Archivo (nombres 8.3 en mayúsculas, FAT16)
// -----------------------------------------------------------------------
#define COMETA_L2_PREFIJO "L2_"
#define COMETA_L2_EXT     ".CSV"

#endif  // COMETA_CONFIG_ADALOGGER_H
