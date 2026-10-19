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
// NAV-PVT llega solo (setAutoPVT), una vez por segundo, y getPVT() no
// espera: lee lo que haya llegado. Un tic sin NAV-PVT nuevo es normal
// (el receptor y el tic van a 1 Hz cada uno, sin sincronía); pasar esto
// sin ninguno no: se recupera el bus y se reinicializa el GPS, que
// vuelve a recibir el modo Airborne (REQUISITOS.md §3.1, §3.3). Mismo
// valor que en el Teensy.
#define COMETA_GPS_SIN_PVT_MAX_MS 3000UL
// Si la reinicialización falla, begin() se reintenta cada tanto.
#define COMETA_GPS_REINTENTO_MS 30000UL
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
// Si la SD no monta al arrancar, o una escritura falla en vuelo, se
// vuelve a montar cada tanto y se sigue en un L2_nnn.CSV nuevo
// (REQUISITOS.md §4.1). SD.begin() con la tarjeta ausente tarda hasta
// ~2 s: no se intenta en cada tic.
#define COMETA_SD_REINTENTO_MS 30000UL

// -----------------------------------------------------------------------
// §2 — LEDs de estado de la placa, para revisar la sonda antes de
// cerrarla sin conectar la PC: en cada tic destella UNO de los dos.
// Verde (pin 8, junto a la ranura de la SD) = la fila se escribió en la
// SD; rojo (pin 13) = la SD no está grabando.
// -----------------------------------------------------------------------
#define COMETA_LED_SD_OK_PIN    8
#define COMETA_LED_SD_ERROR_PIN LED_BUILTIN
// Duración del destello: unos 2 mA durante 30 ms por segundo, ~0,06 mA
// de promedio.
#define COMETA_LED_DESTELLO_MS  30UL

// -----------------------------------------------------------------------
// §2.1, §3.10 — Tensión de batería por ADC. El pack se conecta por el pin
// USB a través de un diodo (1N5819/1N5817), nunca por el JST BAT: el pin
// A7 de fábrica del Feather M0 (que mide la batería del JST) NO sirve acá.
// Divisor propio 100 kΩ / 100 kΩ en A2, colocado sobre el positivo del
// pack ANTES del diodo, con un cerámico de 100 nF entre el punto medio
// del divisor y GND (REQUISITOS.md §2.1).
// A2 y no A1: en el banco, con adalogger_pines, el mismo cable del
// divisor no daba nada en A1 (valores al azar, como un pin libre) y sí
// en A2. El pin A1 de este Feather no hace contacto.
// -----------------------------------------------------------------------
#define COMETA_VBATT_ADC_PIN        A2
#define COMETA_VBATT_DIVISOR_FACTOR 2.0
// ADC del SAMD21: referencia 3,3 V, lectura a 12 bits.
#define COMETA_ADC_VREF 3.3
#define COMETA_ADC_BITS 12
// Lecturas promediadas por fila: el ruido del ADC y los picos de consumo
// del E220 al transmitir mueven la última cifra de v_batt.
#define COMETA_VBATT_MUESTRAS 8

// -----------------------------------------------------------------------
// §3.11 — Línea "ESTADO:" por Serial cada tanto: lo que setup() informa
// una sola vez (modo Airborne, causa del reinicio, archivo de la SD) se
// pierde si el monitor serie se abre después del arranque, que es lo de
// siempre con el USB nativo del Feather M0. Esta placa no tiene META.
// -----------------------------------------------------------------------
#define COMETA_ESTADO_PERIODO_MS 10000UL

// -----------------------------------------------------------------------
// §2.2 — Telemetría por radio: E220 (LoRa 920 MHz) en Serial1, TX (D1)
// -> RXD, RX (D0) <- TXD. M0 y M1 a GND: modo normal (transparente); la
// configuración del módulo (canal, velocidad de aire, potencia) se hace
// antes del montaje y el firmware no la puede cambiar.
// -----------------------------------------------------------------------
#define COMETA_E220_SERIAL Serial1
// TODO VERIFICAR: tiene que coincidir con la velocidad UART guardada en
// el E220 (9600 8N1 de fábrica) (REQUISITOS.md §2.2).
#define COMETA_E220_BAUD   9600UL

// Trama de radio (REQUISITOS.md §2.2): la fila L2 con id y contador en
// vez de t_ms, y utc como hhmmss.
#define COMETA_RADIO_ID "C2"
// TODO VERIFICAR en banco: si el E220 interfiere con el Geiger (comparar
// cpi con la radio encendida y apagada), subir a 10000UL.
#define COMETA_RADIO_PERIODO_MS 5000UL
// Largo máximo de la trama, sin CR LF: el peor caso es ~62 caracteres.
#define COMETA_RADIO_LARGO_MAX 80

// -----------------------------------------------------------------------
// §4.1 — Archivo (nombres 8.3 en mayúsculas, FAT16)
// -----------------------------------------------------------------------
#define COMETA_L2_PREFIJO "L2_"
#define COMETA_L2_EXT     ".CSV"
// nnn va de 000 a 999.
#define COMETA_L2_MAX_ARCHIVOS 1000

// -----------------------------------------------------------------------
// §3.11 — Watchdog (Adafruit_SleepyDog, timeout en segundos)
// -----------------------------------------------------------------------
#define COMETA_WATCHDOG_TIMEOUT_S 8

#endif  // COMETA_CONFIG_ADALOGGER_H
