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
#include <Adafruit_MAX31865.h>

// -----------------------------------------------------------------------
// Centinela para el único valor [VERIFICAR] que queda: el timeout de
// recuperación I²C. Es un valor imposible
// en la práctica, para que el placeholder sea evidente en vez de un
// número creíble.
// -----------------------------------------------------------------------
#define COMETA_MS_VERIFICAR (-1L)  // TODO VERIFICAR: duración sin confirmar

// -----------------------------------------------------------------------
// §1 — Estructura / reloj del sistema
// -----------------------------------------------------------------------
// Reloj del Teensy a 150 MHz (rango aceptable 150-300 MHz). Se fija con
// set_arm_clock() al arrancar; la frecuencia que de verdad quedó va a
// META como cpu_hz (REQUISITOS.md §3.8): ahí se verifica.
#define COMETA_CPU_CLOCK_HZ    150000000UL

// -----------------------------------------------------------------------
// §3.1 — GPS en modo Airborne <1g (SAM-M8Q, generación M8: CFG-NAV5 legacy,
// NO setVal/VALSET)
// -----------------------------------------------------------------------
#define COMETA_GPS_I2C_ADDR         0x42
// NAV-PVT llega solo (setAutoPVT), una vez por segundo, y getPVT() no
// espera (§1.2). Si pasa esto sin ninguno nuevo, la lectura cuenta como
// fallida (REQUISITOS.md §1.3).
#define COMETA_GPS_SIN_PVT_MAX_MS   3000UL
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
// Tick rápido: ICM-20948 y buffer/disparo de la ventana IMU, derivado de
// COMETA_IMU_TASA_HZ (§7) para no duplicar la tasa de muestreo en dos
// constantes: a 100 Hz da 10 ms.
#define COMETA_TICK_IMU_MS (1000UL / COMETA_IMU_TASA_HZ)
// Tick lento: el resto de los sensores y el log SCI a 1 Hz
// (REQUISITOS.md §4.1).
#define COMETA_TICK_SCI_MS 1000UL

// -----------------------------------------------------------------------
// §3.3 — Recuperación del bus I²C (9 pulsos en SCL si SDA queda baja)
// -----------------------------------------------------------------------
#define COMETA_I2C_RECOVERY_PULSOS_SCL 9
// Bus Wire del Teensy 4.1 (REQUISITOS.md §5): la recuperación los toma
// como GPIO mientras el bus está apagado.
#define COMETA_I2C_SDA_PIN 18
#define COMETA_I2C_SCL_PIN 19
// Bus del GPS. En el banco, el SAM-M8Q en el bus compartido dejó SDA baja
// cada 1-3 min y se llevaba con él al ICM-20948 y al LTR390; sin el GPS,
// 17 min sin una sola recuperación. Con 1 va solo en Wire1 (SDA1 = 17,
// SCL1 = 16, libres en §5): si se cuelga, se pierde solo el GPS y se
// recupera su bus sin tocar los demás; 19 min sin recuperaciones en el
// banco (REQUISITOS.md §5). 0 solo para un GPS todavía en la cadena.
#ifndef COMETA_GPS_EN_WIRE1
#define COMETA_GPS_EN_WIRE1 1
#endif
#if COMETA_GPS_EN_WIRE1
#define COMETA_GPS_WIRE    Wire1
#define COMETA_GPS_SDA_PIN 17
#define COMETA_GPS_SCL_PIN 16
#else
#define COMETA_GPS_WIRE    Wire
#define COMETA_GPS_SDA_PIN COMETA_I2C_SDA_PIN
#define COMETA_GPS_SCL_PIN COMETA_I2C_SCL_PIN
#endif
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
// setAmbientPressure() relanza la medición continua: no se manda en cada
// tic, solo cuando la presión cambió al menos esto desde el último envío.
#define COMETA_SCD30_PRESION_DELTA_MBAR    5
// El SCD30 da un dato cada 2 s: dataAvailable() == false es lo normal en
// un tic de cada dos. Recién si pasa esto sin ningún dato nuevo se cuenta
// como lectura fallida (REQUISITOS.md §1.3).
#define COMETA_SCD30_SIN_DATO_MAX_MS       6000UL

// -----------------------------------------------------------------------
// §5 — MS8607 (I²C 0x76 + 0x40, fondo de escala 10 hPa)
// -----------------------------------------------------------------------
#define COMETA_MS8607_I2C_ADDR_PRESION_TEMP 0x76
#define COMETA_MS8607_I2C_ADDR_HUMEDAD      0x40

// -----------------------------------------------------------------------
// §5 — SHT45 (I²C 0x44, en el brazo exterior). Adafruit_SHT4x no recibe
// la dirección por parámetro: la tiene fija en SHT4x_DEFAULT_ADDR, que
// vale lo mismo. La constante queda como documentación del bus (es la
// que se compara con el escaneo I²C que va a META, §4.7), no como
// parámetro de sensores/sht45.cpp.
// -----------------------------------------------------------------------
#define COMETA_SHT45_I2C_ADDR 0x44

// -----------------------------------------------------------------------
// §5 — ICM-20948 (I²C 0x69, breakout con AD0 alto; ±16 g). La tasa de
// muestreo es COMETA_IMU_TASA_HZ (§7): es la misma tasa a la que se llena
// el buffer circular de la ventana IMU.
// -----------------------------------------------------------------------
#define COMETA_ICM20948_I2C_ADDR   0x69
#define COMETA_ICM20948_RANGO_G    16
#define COMETA_ICM20948_RANGO_DPS  2000
// FIFO del chip (§7): acelerómetro y giróscopo, 12 bytes por muestra.
// Con más de esto acumulado, o con un resto que no cierra en 12, la FIFO
// se da por desalineada y se reinicia (se pierden esas muestras, se
// cuenta en META).
#define COMETA_ICM20948_FIFO_MAX_BYTES 480
// El magnetómetro no entra en la FIFO: se relee cada tantos tics de
// 10 ms (10 = 10 Hz) y cada muestra lleva el último valor.
#define COMETA_ICM20948_MAG_CADA_N_TICS 10

// -----------------------------------------------------------------------
// §5 — LTR390 (I²C 0x53)
// -----------------------------------------------------------------------
#define COMETA_LTR390_I2C_ADDR 0x53

// -----------------------------------------------------------------------
// §5 — 2x MAX31865 (SPI) para sondas PT1000, A y B. A es la que da la T
// exterior: con ella se decide el encendido del PMS5003 (§6) y q_pms.
// -----------------------------------------------------------------------
#define COMETA_MAX31865_RREF        4300.0
#define COMETA_MAX31865_RNOMINAL    1000.0
#define COMETA_MAX31865_CS_A_PIN 10  // A, en SPI (MOSI 11, MISO 12, SCK 13)
#define COMETA_MAX31865_CS_B_PIN 9   // B, en SPI1
// B va en su propio bus, SPI1: cada MAX31865 se cablea directo a sus
// pines del Teensy, sin repartir MOSI/MISO/SCK entre las dos placas, y
// si uno deja su bus mal el otro no se entera. MISO1 por defecto es el
// pin 1, que es el TX1 del PMS5003: se usa la alternativa, el 39.
#define COMETA_MAX31865_B_SPI      SPI1
#define COMETA_MAX31865_B_MOSI_PIN 26
#define COMETA_MAX31865_B_MISO_PIN 39
#define COMETA_MAX31865_B_SCK_PIN  27
#define COMETA_MAX31865_WIRING      MAX31865_3WIRE

// -----------------------------------------------------------------------
// §5 — Geiger GGreg20 (interrupción vía optoacoplador)
// -----------------------------------------------------------------------
#define COMETA_GEIGER_PIN 3  // interrupción vía optoacoplador

// -----------------------------------------------------------------------
// §3.6, §6 — PMS5003 (UART Serial1, controlado por MOSFET de canal N que
// corta la masa del sensor)
// -----------------------------------------------------------------------
#define COMETA_PMS5003_SERIAL   Serial1
#define COMETA_PMS5003_RX_PIN   0
#define COMETA_PMS5003_TX_PIN   1
#define COMETA_PMS5003_BAUD     9600
#define COMETA_PMS5003_MOSFET_PIN 4  // control del MOSFET de alimentación
// Espera tras subir el MOSFET antes de abrir Serial1 (asentamiento del
// regulador del PMS).
#define COMETA_PMS5003_MOSFET_SETTLE_MS 50UL
#define COMETA_PMS_ALTITUD_TECHO_M  5000.0
#define COMETA_PMS_APAGAR_BAJO_C    (-15.0)
#define COMETA_PMS_ENCENDER_SOBRE_C (-12.0)
// Primeras muestras a descartar tras cada encendido: el ventilador tarda
// en estabilizarse (REQUISITOS.md §6).
#define COMETA_PMS_CALENTAMIENTO_S 30UL
// En modo activo el PMS5003 manda una trama cada 0,2–2,3 s. Encendido y
// ya caliente, si pasa esto sin ninguna trama válida, la lectura cuenta
// como fallida (REQUISITOS.md §1.3); tras 5 fallos el gestor lo marca
// ausente y el reintento de iniciar() le corta y devuelve la masa.
#define COMETA_PMS_SIN_TRAMA_MAX_MS 5000UL

// -----------------------------------------------------------------------
// §2.1, §3.10, §4.3 — Tensión de batería por ADC. Divisor 100 kΩ / 33 kΩ
// en A0, colocado DESPUÉS del interruptor del nivel 3 (mide lo que
// realmente llega al Teensy), con un cerámico de 100 nF entre el punto
// medio del divisor y GND (REQUISITOS.md §2.1).
// -----------------------------------------------------------------------
#define COMETA_VBATT_ADC_PIN        A0
#define COMETA_VBATT_DIVISOR_FACTOR 4.03
// ADC del Teensy 4.1: referencia 3,3 V, lectura a 12 bits.
#define COMETA_ADC_VREF 3.3
#define COMETA_ADC_BITS 12

// -----------------------------------------------------------------------
// §4.4 — Flags de calidad (0 = en especificación, 1 = fuera)
// -----------------------------------------------------------------------
#define COMETA_Q_PMS_TEMP_PTA_MIN_C   (-10.0)  // PT1000 A (exterior)
#define COMETA_Q_RH_TEMP_SHT_MIN_C    (-40.0)  // SHT45
#define COMETA_Q_PTA_TEMP_MIN_C       (-50.0)  // PT1000 A
#define COMETA_Q_PTB_TEMP_MIN_C       (-50.0)  // PT1000 B
#define COMETA_Q_P_MIN_HPA            (10.0)   // MS8607

// -----------------------------------------------------------------------
// §7 — Ventana IMU alrededor del estallido (umbrales aquí para poder
// probarlos en tierra). Los de armado y disparo van con #ifndef: el
// entorno teensy41_prueba_imu de platformio.ini los cambia por
// build_flags para disparar la ventana en el banco, sin tocar este
// archivo.
// -----------------------------------------------------------------------
#define COMETA_IMU_TASA_HZ                 100
#define COMETA_IMU_BUFFER_SEGUNDOS         30
#ifndef COMETA_IMU_ARMADO_ALTITUD_M
#define COMETA_IMU_ARMADO_ALTITUD_M        10000.0
#endif
#ifndef COMETA_IMU_ARMADO_PRESION_HPA
#define COMETA_IMU_ARMADO_PRESION_HPA      250.0
#endif
#ifndef COMETA_IMU_DISPARO_CAIDA_LIBRE_G
#define COMETA_IMU_DISPARO_CAIDA_LIBRE_G   0.3
#endif
#ifndef COMETA_IMU_DISPARO_CAIDA_LIBRE_MS
#define COMETA_IMU_DISPARO_CAIDA_LIBRE_MS  1000UL   // sostenido >= 1 s
#endif
#define COMETA_IMU_DISPARO_VZ_MS           (-10.0)
#define COMETA_IMU_DISPARO_VZ_FIXES        3        // fixes consecutivos
#define COMETA_IMU_VENTANA_PRE_S           20
#define COMETA_IMU_VENTANA_POST_S          90
// Filas de IMU_nnn.CSV escritas por tic de 10 ms después del disparo: más
// que las que entran (1 por tic), para ponerse al día con los 20 s
// previos (2000 filas en ~7 s) sin bloquear el tic.
#define COMETA_IMU_FILAS_POR_TIC           4
#define COMETA_IMU_FLUSH_CADA_N_FILAS      100

// -----------------------------------------------------------------------
// §4.1 — Archivos (SD BUILTIN_SDCARD, nombres 8.3 en mayúsculas, FAT16)
// -----------------------------------------------------------------------
#define COMETA_SCI_PREFIJO  "SCI_"
#define COMETA_IMU_PREFIJO  "IMU_"
#define COMETA_META_PREFIJO "META_"
#define COMETA_SCI_EXT  ".CSV"
#define COMETA_IMU_EXT  ".CSV"
#define COMETA_META_EXT ".TXT"
// nnn va de 000 a 999: tres cifras para que el nombre siga siendo 8.3
// ("META_nnn" son 8 caracteres).
#define COMETA_MAX_ARCHIVOS 1000
// Si la SD no montó al arrancar, cada cuánto se vuelve a intentar. Tras
// un reinicio por watchdog la tarjeta no se queda sin alimentación y
// puede seguir trabada unos segundos; o se insertó con la placa ya
// encendida. Cada intento sin tarjeta tarda pocos ms.
#define COMETA_SD_REINTENTO_MS 10000UL

// -----------------------------------------------------------------------
// §1.3 — Sensores que no responden: reintento de iniciar() y umbral de
// lecturas fallidas consecutivas antes de marcar el sensor ausente
// -----------------------------------------------------------------------
#define COMETA_SENSOR_REINTENTO_MS 30000UL
#define COMETA_SENSOR_MAX_FALLOS   5

// -----------------------------------------------------------------------
// §1.2 — Presupuesto de bloqueo de un módulo de sensor. Una llamada a
// actualizar() en el tic de 1000 ms no puede bloquear más que esto; una
// conversión más larga se parte en dos pasadas, arranque y recogida. En el tic de 10 ms no se bloquea nada.
// -----------------------------------------------------------------------
#define COMETA_BLOQUEO_MAX_MS 20UL

// -----------------------------------------------------------------------
// §3.11 — Watchdog (WDT_T4, timeout en segundos)
// -----------------------------------------------------------------------
#define COMETA_WATCHDOG_TIMEOUT_S 8

#endif  // COMETA_CONFIG_TEENSY_H
