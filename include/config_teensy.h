// config_teensy.h
//
// Constantes de la cadena científica (Teensy 4.1, nivel 3). Ningún número
// mágico en el código: todo lo que aparece aquí está definido en
// REQUISITOS.md. Los valores marcados [VERIFICAR] no están confirmados:
// no se inventan, se miden o se consultan en la hoja de datos antes del
// vuelo.

#ifndef COMETA_CONFIG_TEENSY_H
#define COMETA_CONFIG_TEENSY_H

#include <Arduino.h>
#include <math.h>

// -----------------------------------------------------------------------
// Centinelas para valores [VERIFICAR]: valores imposibles en la práctica,
// para que el placeholder sea evidente en vez de un número creíble.
// -----------------------------------------------------------------------
#define COMETA_PIN_VERIFICAR   (-1)     // TODO VERIFICAR: pin sin confirmar
#define COMETA_MS_VERIFICAR    (-1L)    // TODO VERIFICAR: duración sin confirmar
#define COMETA_F_VERIFICAR     (NAN)    // TODO VERIFICAR: valor físico sin confirmar
#define COMETA_MODO_VERIFICAR  (-1)     // TODO VERIFICAR: modo/enum sin confirmar

// -----------------------------------------------------------------------
// §1 — Estructura / reloj del sistema
// -----------------------------------------------------------------------
// Reloj del Teensy a 150 MHz (rango aceptable 150-300 MHz).
#define COMETA_CPU_CLOCK_HZ    150000000UL
// TODO VERIFICAR: confirmar en el core instalado qué frecuencias acepta
// set_arm_clock() (REQUISITOS.md §3.8).

// -----------------------------------------------------------------------
// §3.1 — GPS en modo Airborne <1g (SAM-M8Q, generación M8: CFG-NAV5 legacy,
// NO setVal/VALSET)
// -----------------------------------------------------------------------
#define COMETA_GPS_I2C_ADDR         0x42
// TODO VERIFICAR: ¿DYN_MODEL_AIRBORNE1g se guarda en flash o solo en BBR?
// (REQUISITOS.md §3.1). Se decide al implementar inicializarGPS(): no es
// un valor que se pueda fijar de antemano en una constante.

// -----------------------------------------------------------------------
// §3.2 — flush() periódico del log
// -----------------------------------------------------------------------
#define COMETA_FLUSH_CADA_N_MUESTRAS 10

// -----------------------------------------------------------------------
// §2, §7 — Periodos de loop() no bloqueante (ticks basados en millis(),
// sin delay())
// -----------------------------------------------------------------------
// Tick rápido: ICM-20948 y buffer/disparo de la ventana IMU a 100 Hz
// (REQUISITOS.md §7).
#define COMETA_TICK_IMU_MS 10UL
// Tick lento: el resto de los sensores y el log SCI a 1 Hz
// (REQUISITOS.md §4.1).
#define COMETA_TICK_SCI_MS 1000UL

// -----------------------------------------------------------------------
// §3.3 — Recuperación del bus I²C (9 pulsos en SCL si SDA queda baja)
// -----------------------------------------------------------------------
#define COMETA_I2C_RECOVERY_PULSOS_SCL 9
// TODO VERIFICAR con la prueba 13: timeout por encima del peor caso de
// clock stretching del SCD30 (3 ms es insuficiente).
#define COMETA_I2C_TIMEOUT_MS COMETA_MS_VERIFICAR

// -----------------------------------------------------------------------
// §3.4, §3.5, §5 — SCD30 (I²C 0x61)
// -----------------------------------------------------------------------
#define COMETA_SCD30_I2C_ADDR              0x61
#define COMETA_SCD30_PRESION_MIN_MBAR      700
#define COMETA_SCD30_PRESION_MAX_MBAR      1400
#define COMETA_SCD30_AUTO_SELF_CALIBRATION false

// -----------------------------------------------------------------------
// §5 — MS8607 (I²C 0x76 + 0x40, fondo de escala 10 hPa)
// -----------------------------------------------------------------------
#define COMETA_MS8607_I2C_ADDR_PRESION_TEMP 0x76
#define COMETA_MS8607_I2C_ADDR_HUMEDAD      0x40

// -----------------------------------------------------------------------
// §5 — SHT45 (I²C 0x44, en el brazo exterior)
// -----------------------------------------------------------------------
#define COMETA_SHT45_I2C_ADDR 0x44

// -----------------------------------------------------------------------
// §5 — ICM-20948 (I²C 0x69, breakout con AD0 alto; ±16 g). La tasa de
// muestreo es COMETA_IMU_TASA_HZ (§7): es la misma tasa a la que se llena
// el buffer circular de la ventana IMU.
// -----------------------------------------------------------------------
#define COMETA_ICM20948_I2C_ADDR   0x69
#define COMETA_ICM20948_RANGO_G    16

// -----------------------------------------------------------------------
// §5 — LTR390 (I²C 0x53)
// -----------------------------------------------------------------------
#define COMETA_LTR390_I2C_ADDR 0x53

// -----------------------------------------------------------------------
// §5 — 2x MAX31865 (SPI) para sondas PT1000 (brazo exterior y tubo)
// -----------------------------------------------------------------------
#define COMETA_MAX31865_RREF     4300.0
#define COMETA_MAX31865_RNOMINAL 1000.0
// TODO VERIFICAR pines: CS distinto para cada MAX31865 (REQUISITOS.md §5).
#define COMETA_MAX31865_CS_ARM_PIN  COMETA_PIN_VERIFICAR
#define COMETA_MAX31865_CS_TUBO_PIN COMETA_PIN_VERIFICAR
// TODO VERIFICAR con los puentes soldados en la placa: MAX31865_2WIRE,
// MAX31865_3WIRE o MAX31865_4WIRE (REQUISITOS.md §5).
#define COMETA_MAX31865_WIRING COMETA_MODO_VERIFICAR

// -----------------------------------------------------------------------
// §5 — 4x DS18B20 (1-Wire, pull-up 4.7 kΩ). Resolución 12 bits,
// setWaitForConversion(false): nunca bloquear el loop 750 ms.
// -----------------------------------------------------------------------
// TODO VERIFICAR pin: bus 1-Wire compartido por las cuatro sondas.
#define COMETA_DS18B20_BUS_PIN COMETA_PIN_VERIFICAR
#define COMETA_DS18B20_RESOLUCION_BITS 12

// TODO VERIFICAR: identificar las cuatro ROM antes del montaje (caja,
// pilas, centro del volumen, SCD30) y completar aquí (REQUISITOS.md §5).
#define COMETA_DS18B20_ROM_CASSA   { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define COMETA_DS18B20_ROM_PILE    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define COMETA_DS18B20_ROM_CENTRO  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define COMETA_DS18B20_ROM_SCD     { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// -----------------------------------------------------------------------
// §5 — Geiger GGreg20 (interrupción vía optoacoplador)
// -----------------------------------------------------------------------
// TODO VERIFICAR pin de interrupción (REQUISITOS.md §5).
#define COMETA_GEIGER_PIN COMETA_PIN_VERIFICAR

// -----------------------------------------------------------------------
// §3.6, §6 — PMS5003 (UART Serial1, controlado por MOSFET)
// -----------------------------------------------------------------------
#define COMETA_PMS5003_SERIAL Serial1
// TODO VERIFICAR pin: control del MOSFET de alimentación del PMS5003.
#define COMETA_PMS5003_MOSFET_PIN COMETA_PIN_VERIFICAR
#define COMETA_PMS_ALTITUD_TECHO_M  5000.0
#define COMETA_PMS_APAGAR_BAJO_C    (-15.0)
#define COMETA_PMS_ENCENDER_SOBRE_C (-12.0)

// -----------------------------------------------------------------------
// §3.10, §5, §4.3 — Tensión de batería por ADC
// -----------------------------------------------------------------------
// TODO VERIFICAR pin y divisor resistivo (REQUISITOS.md §3.10).
#define COMETA_VBATT_ADC_PIN        COMETA_PIN_VERIFICAR
#define COMETA_VBATT_DIVISOR_FACTOR COMETA_F_VERIFICAR

// -----------------------------------------------------------------------
// §4.4 — Flags de calidad (0 = en especificación, 1 = fuera)
// -----------------------------------------------------------------------
#define COMETA_Q_PMS_TEMP_ARM_MIN_C   (-10.0)  // PT1000 brazo
#define COMETA_Q_RH_TEMP_SHT_MIN_C    (-40.0)  // SHT45
#define COMETA_Q_ARM_TEMP_MIN_C       (-50.0)  // PT1000 brazo
#define COMETA_Q_TUBO_TEMP_MIN_C      (-50.0)  // PT1000 tubo
#define COMETA_Q_P_MIN_HPA            (10.0)   // MS8607

// -----------------------------------------------------------------------
// §7 — Ventana IMU alrededor del estallido (umbrales aquí para poder
// probarlos en tierra)
// -----------------------------------------------------------------------
#define COMETA_IMU_TASA_HZ                 100
#define COMETA_IMU_BUFFER_SEGUNDOS         30
#define COMETA_IMU_ARMADO_ALTITUD_M        10000.0
#define COMETA_IMU_ARMADO_PRESION_HPA      250.0
#define COMETA_IMU_DISPARO_CAIDA_LIBRE_G   0.3
#define COMETA_IMU_DISPARO_CAIDA_LIBRE_MS  1000UL   // sostenido >= 1 s
#define COMETA_IMU_DISPARO_VZ_MS           (-10.0)
#define COMETA_IMU_DISPARO_VZ_FIXES        3        // fixes consecutivos
#define COMETA_IMU_VENTANA_PRE_S           20
#define COMETA_IMU_VENTANA_POST_S          90

// -----------------------------------------------------------------------
// §4.1 — Archivos (SD BUILTIN_SDCARD, nombres 8.3 en mayúsculas, FAT16)
// -----------------------------------------------------------------------
#define COMETA_SCI_PREFIJO  "SCI_"
#define COMETA_IMU_PREFIJO  "IMU_"
#define COMETA_META_PREFIJO "META_"
#define COMETA_SCI_EXT  ".CSV"
#define COMETA_IMU_EXT  ".CSV"
#define COMETA_META_EXT ".TXT"

#endif  // COMETA_CONFIG_TEENSY_H
