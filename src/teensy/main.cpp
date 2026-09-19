// COMETA · Teensy 4.1 (nivel 3) — cadena científica completa
//
// Contrato del firmware (REQUISITOS.md §1.2, §1.3): cada sensor es un
// módulo en src/teensy/sensores/ con la interfaz común iniciar()/
// actualizar()/llenarFila(). main.cpp arma la fila SCI, recorre los
// módulos con un gestor genérico (reintentos y ausencia, REQUISITOS.md
// §1.3) y escribe el log. Solo SHT45 está implementado de verdad: el
// resto son cuerpos vacíos a completar por los estudiantes.

#include <Arduino.h>
#include <SD.h>
#include <Watchdog_t4.h>

#include "config_teensy.h"
#include "log_format.h"

#include "sensores/ds18b20.h"
#include "sensores/geiger.h"
#include "sensores/gps.h"
#include "sensores/icm20948.h"
#include "sensores/ltr390.h"
#include "sensores/max31865.h"
#include "sensores/ms8607.h"
#include "sensores/pms5003.h"
#include "sensores/scd30.h"
#include "sensores/sht45.h"

// -----------------------------------------------------------------------
// Archivos de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoSCI;
File archivoIMU;
File archivoMETA;

// Fila SCI en construcción durante el tic de 1000 ms (REQUISITOS.md
// §4.3). limpiarFilaSCI() la vacía al principio de cada tic; cada
// módulo de sensor completa solo sus propios campos.
FilaSCI filaSCI;

// -----------------------------------------------------------------------
// Watchdog (REQUISITOS.md §3.11): WDT_T4, timeout COMETA_WATCHDOG_TIMEOUT_S.
// Se alimenta una sola vez por pasada de loop(), nunca dentro de un
// módulo de sensor.
// -----------------------------------------------------------------------
WDT_T4<WDT1> wdt;

void configurarWatchdog() {
  WDT_timings_t config = {};
  config.timeout = COMETA_WATCHDOG_TIMEOUT_S;
  wdt.begin(config);
}

// -----------------------------------------------------------------------
// Reloj y arranque
// -----------------------------------------------------------------------

// Fija el reloj de la CPU a COMETA_CPU_CLOCK_HZ (REQUISITOS.md §3.8).
void configurarRelojCPU() {
  // TODO
}

// Monta la SD (BUILTIN_SDCARD) y busca el primer número nnn libre para
// los archivos de esta corrida (REQUISITOS.md §4.1).
void inicializarSD() {
  // TODO
}

// Crea SCI_nnn.CSV y escribe su encabezado (REQUISITOS.md §4.1, §4.3).
void abrirArchivoSCI() {
  // TODO: crear el archivo con el primer nnn libre.
  escribirEncabezadoSCI(archivoSCI);
}

// Crea META_nnn.TXT (datos que no cambian durante el vuelo,
// REQUISITOS.md §4.1, §4.7).
void abrirArchivoMETA() {
  // TODO
}

// -----------------------------------------------------------------------
// Verificación de configuración (valores [VERIFICAR] de config_teensy.h
// todavía sin confirmar)
// -----------------------------------------------------------------------

// Devuelve true si rom sigue siendo el centinela de ocho ceros: la sonda
// todavía no fue identificada por ROM (REQUISITOS.md §5).
bool romEsCentinela(const uint8_t rom[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    if (rom[i] != 0x00) {
      return false;
    }
  }
  return true;
}

// Escribe "CONFIG INCOMPLETA: <nombre>" en META por cada centinela sin
// confirmar (los valores [VERIFICAR] no se inventan).
void reportarConfigIncompleta(const char *nombre) {
  archivoMETA.print("CONFIG INCOMPLETA: ");
  archivoMETA.println(nombre);
}

// Recorre las constantes marcadas [VERIFICAR] en config_teensy.h y
// registra en META cuáles siguen sin confirmar antes de volar. Los demás
// centinelas de config_teensy.h ya se fijaron a valores decididos
// (REQUISITOS.md §2.1, §5): solo quedan el timeout I²C y las cuatro ROM
// de los DS18B20.
void verificarConfiguracion() {
  {
    const uint8_t romCassa[8] = COMETA_DS18B20_ROM_CASSA;
    if (romEsCentinela(romCassa)) {
      reportarConfigIncompleta("DS18B20_ROM_CASSA");
    }
  }
  {
    const uint8_t romPile[8] = COMETA_DS18B20_ROM_PILE;
    if (romEsCentinela(romPile)) {
      reportarConfigIncompleta("DS18B20_ROM_PILE");
    }
  }
  {
    const uint8_t romCentro[8] = COMETA_DS18B20_ROM_CENTRO;
    if (romEsCentinela(romCentro)) {
      reportarConfigIncompleta("DS18B20_ROM_CENTRO");
    }
  }
  {
    const uint8_t romScd[8] = COMETA_DS18B20_ROM_SCD;
    if (romEsCentinela(romScd)) {
      reportarConfigIncompleta("DS18B20_ROM_SCD");
    }
  }
  if (COMETA_I2C_TIMEOUT_MS == COMETA_MS_VERIFICAR) {
    reportarConfigIncompleta("I2C_TIMEOUT_MS");
  }
}

// -----------------------------------------------------------------------
// Sensores que no responden (REQUISITOS.md §1.3)
//
// Lógica genérica, en un único lugar: cada módulo de sensor solo
// implementa iniciar()/actualizar()/llenarFila() (REQUISITOS.md §1.2).
// Si iniciar() falla, el sensor queda ausente (sus celdas quedan vacías
// y el loop sigue) y se reintenta cada COMETA_SENSOR_REINTENTO_MS.
// Si ya estaba presente, 5 lecturas fallidas consecutivas de
// actualizar() lo marcan ausente con la misma regla. reinit cuenta los
// intentos de iniciar() de todos los sensores.
// -----------------------------------------------------------------------

struct EstadoSensor {
  const char *nombre;
  bool (*iniciar)();
  bool (*actualizar)(uint32_t);
  void (*llenarFila)(FilaSCI &);
  bool presente;
  uint32_t proximoReintentoMs;
  uint8_t fallosConsecutivos;
  uint32_t reinit;
};

// Cuenta total de reintentos de iniciar(), de todos los sensores
// (columna `reinit` de SCI, REQUISITOS.md §1.3, §4.3).
uint32_t reinitTotal = 0;

// Todos los sensores menos el ICM-20948, que se actualiza aparte en el
// tic de 10 ms (ver loop()) porque su FIFO hay que vaciarla a 100 Hz, no
// a 1 Hz como el resto.
EstadoSensor sensores[] = {
    {"SCD30", SensorSCD30::iniciar, SensorSCD30::actualizar,
     SensorSCD30::llenarFila, false, 0, 0, 0},
    {"MS8607", SensorMS8607::iniciar, SensorMS8607::actualizar,
     SensorMS8607::llenarFila, false, 0, 0, 0},
    {"SHT45", SensorSHT45::iniciar, SensorSHT45::actualizar,
     SensorSHT45::llenarFila, false, 0, 0, 0},
    {"LTR390", SensorLTR390::iniciar, SensorLTR390::actualizar,
     SensorLTR390::llenarFila, false, 0, 0, 0},
    {"MAX31865_BRAZO", SensorPT1000Brazo::iniciar,
     SensorPT1000Brazo::actualizar, SensorPT1000Brazo::llenarFila, false, 0,
     0, 0},
    {"MAX31865_TUBO", SensorPT1000Tubo::iniciar, SensorPT1000Tubo::actualizar,
     SensorPT1000Tubo::llenarFila, false, 0, 0, 0},
    {"DS18B20", SensorDS18B20::iniciar, SensorDS18B20::actualizar,
     SensorDS18B20::llenarFila, false, 0, 0, 0},
    {"PMS5003", SensorPMS5003::iniciar, SensorPMS5003::actualizar,
     SensorPMS5003::llenarFila, false, 0, 0, 0},
    {"GEIGER", SensorGeiger::iniciar, SensorGeiger::actualizar,
     SensorGeiger::llenarFila, false, 0, 0, 0},
    {"GPS", SensorGPS::iniciar, SensorGPS::actualizar, SensorGPS::llenarFila,
     false, 0, 0, 0},
};
const size_t COMETA_NUM_SENSORES = sizeof(sensores) / sizeof(sensores[0]);

// ICM-20948: mismo EstadoSensor, tic aparte (100 Hz).
EstadoSensor estadoICM20948 = {"ICM20948", SensorICM20948::iniciar,
                                SensorICM20948::actualizar,
                                SensorICM20948::llenarFila, false, 0, 0, 0};

// Primer intento de iniciar(), al arrancar (REQUISITOS.md §1.3): cuenta
// en reinit igual que un reintento, para que escribirMETA() pueda listar
// los sensores ausentes desde el primer momento.
void inicializarSensorAlArranque(EstadoSensor &s) {
  s.reinit++;
  reinitTotal++;
  s.presente = s.iniciar();
  if (!s.presente) {
    s.proximoReintentoMs = COMETA_SENSOR_REINTENTO_MS;
  }
}

// Reintenta iniciar() cada COMETA_SENSOR_REINTENTO_MS si el sensor está
// ausente; si está presente, llama a actualizar() y cuenta fallos
// consecutivos hasta COMETA_SENSOR_MAX_FALLOS (REQUISITOS.md §1.3).
void actualizarSensor(EstadoSensor &s, uint32_t ahora) {
  if (!s.presente) {
    if ((int32_t)(ahora - s.proximoReintentoMs) < 0) {
      return;  // todavía no toca reintentar
    }
    inicializarSensorAlArranque(s);  // mismo conteo de reinit que un reintento
    return;
  }

  if (s.actualizar(ahora)) {
    s.fallosConsecutivos = 0;
  } else {
    s.fallosConsecutivos++;
    if (s.fallosConsecutivos >= COMETA_SENSOR_MAX_FALLOS) {
      s.presente = false;
      s.fallosConsecutivos = 0;
      s.proximoReintentoMs = ahora + COMETA_SENSOR_REINTENTO_MS;
    }
  }
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Cuenta las recuperaciones del bus (columna i2c_recov, REQUISITOS.md
// §4.3). La incrementa recuperarBusI2C() cuando la implemente.
uint32_t contadorRecuperacionesI2C = 0;

// Si SDA queda baja: 9 pulsos en SCL y reinicializar; incrementar
// contadorRecuperacionesI2C (REQUISITOS.md §3.3, §4.3). Se llama desde
// los módulos de sensor I²C (actualizar() de scd30, ms8607, sht45,
// ltr390, icm20948, gps) cuando detectan un error o timeout, no en cada
// vuelta de loop().
void recuperarBusI2C() {
  // TODO
}

// -----------------------------------------------------------------------
// Batería (REQUISITOS.md §3.10, §4.3)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10).
void leerBateria() {
  // TODO
}

// -----------------------------------------------------------------------
// Flags de calidad (REQUISITOS.md §4.4)
// -----------------------------------------------------------------------

// Calcula q_pms, q_rh, q_arm, q_tubo, q_p: datos fuera de especificación
// se marcan, no se descartan (REQUISITOS.md §3.7, §4.4).
void calcularFlagsCalidad() {
  // TODO
}

// -----------------------------------------------------------------------
// Ventana IMU alrededor del estallido (REQUISITOS.md §7)
// -----------------------------------------------------------------------

// Mantiene el buffer circular en RAM a 100 Hz (>= 30 s), siempre activo
// (REQUISITOS.md §7).
void actualizarBufferIMU() {
  // TODO
}

// Evalúa armado (altitud > 10 km, o p_hPa < 250 si el GPS no es válido) y
// el disparo (caída libre |a| < 0.3 g >= 1 s, o respaldo GPS vz_ms < −10
// durante 3 fixes). Una sola ventana, sin rearmado (REQUISITOS.md §7).
void verificarDisparoIMU() {
  // TODO
}

// Recién en el disparo (no antes): crea IMU_nnn.CSV, escribe su
// encabezado y vuelca la ventana (20 s antes, 90 s después) desde el
// buffer circular; registra criterio, t_ms, UTC y altitud en META
// (REQUISITOS.md §4.1, §4.6, §4.7, §7).
void guardarVentanaIMU() {
  // TODO: crear el archivo con el primer nnn libre.
  escribirEncabezadoIMU(archivoIMU);
}

// -----------------------------------------------------------------------
// Log SCI y META
// -----------------------------------------------------------------------

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  // TODO
}

// Escribe en META_nnn.TXT: hash de Git, frecuencia de reloj, RREF/
// RNOMINAL, ROM de cada DS18B20, resultado Airborne, direcciones I²C
// detectadas, los sensores ausentes al arranque (sensores[].presente,
// estadoICM20948.presente, REQUISITOS.md §1.3), la causa del último
// reinicio si el core la expone (si no, [VERIFICAR], REQUISITOS.md
// §3.11), y datos del disparo IMU si ocurrió (REQUISITOS.md §4.7).
void escribirMETA() {
  // TODO
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

void setup() {
  configurarRelojCPU();
  inicializarSD();
  abrirArchivoSCI();
  abrirArchivoMETA();

  verificarConfiguracion();
  configurarWatchdog();

  for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
    inicializarSensorAlArranque(sensores[i]);
  }
  inicializarSensorAlArranque(estadoICM20948);

  escribirMETA();
}

void loop() {
  wdt.feed();  // una sola vez por pasada de loop() (REQUISITOS.md §3.11)

  const unsigned long ahora = millis();
  static unsigned long ultimoTickIMU = 0;
  static unsigned long ultimoTickSCI = 0;

  // Tick a 100 Hz (COMETA_TICK_IMU_MS): IMU y ventana del estallido
  // (REQUISITOS.md §7). Se reprograma sumando el período, no fijándolo a
  // "ahora", para no acumular atraso; si se atrasó más de un período
  // completo, se realinea a "ahora" en vez de intentar recuperarlo.
  if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
    ultimoTickIMU += COMETA_TICK_IMU_MS;
    if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
      ultimoTickIMU = ahora;
    }

    actualizarSensor(estadoICM20948, ahora);
    actualizarBufferIMU();
    verificarDisparoIMU();
    guardarVentanaIMU();
  }

  // Tick a 1 Hz (COMETA_TICK_SCI_MS): resto de sensores y log SCI
  // (REQUISITOS.md §4.1). Misma reprogramación por suma que el tick IMU.
  if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
    ultimoTickSCI += COMETA_TICK_SCI_MS;
    if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
      ultimoTickSCI = ahora;
    }

    limpiarFilaSCI(filaSCI);
    filaSCI.t_ms = ahora;

    for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
      actualizarSensor(sensores[i], ahora);
      if (sensores[i].presente) {
        sensores[i].llenarFila(filaSCI);
      }
    }
    if (estadoICM20948.presente) {
      estadoICM20948.llenarFila(filaSCI);
    }

    // El PMS5003 no se enciende/apaga con el criterio genérico de
    // arriba: decide según altitud y T exterior, con histéresis
    // (REQUISITOS.md §6), no según si "responde".
    SensorPMS5003::controlarEncendido(ahora, filaSCI.alt_m, filaSCI.t_arm_C);

    leerBateria();
    calcularFlagsCalidad();

    filaSCI.i2c_recov = (float)contadorRecuperacionesI2C;
    filaSCI.reinit = (float)reinitTotal;

    escribirFilaSCI(archivoSCI, filaSCI);
    flushLogSiCorresponde();
  }
}
