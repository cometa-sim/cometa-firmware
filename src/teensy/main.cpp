// COMETA · Teensy 4.1 (nivel 3) — cadena científica completa
//
// Impalcatura del firmware: setup()/loop() llaman a funciones declaradas
// pero vacías, una por sensor. La única lógica implementada de verdad es
// la escritura del encabezado CSV (REQUISITOS.md §4).

#include <Arduino.h>
#include <SD.h>

#include "config_teensy.h"
#include "log_format.h"

// -----------------------------------------------------------------------
// Archivos de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoSCI;
File archivoIMU;
File archivoMETA;

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
// GPS — SAM-M8Q (REQUISITOS.md §3.1, §3.9, §4.3, §5)
// -----------------------------------------------------------------------

// Envía DYN_MODEL_AIRBORNE1g (CFG-NAV5 legacy), relee con
// getDynamicModel() y registra el resultado en META (REQUISITOS.md §3.1).
void inicializarGPS() {
  // TODO
}

// Lee posición, altitud, velocidades, sats, pdop, fix y UTC del GPS
// (REQUISITOS.md §3.9, §4.3).
void leerGPS() {
  // TODO
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Si SDA queda baja: 9 pulsos en SCL y reinicializar. Cuenta las
// recuperaciones en i2c_recov (REQUISITOS.md §3.3, §4.3). Se llama desde
// las funciones de lectura I²C (leerGPS(), leerSCD30(), leerMS8607(),
// leerSHT45(), leerLTR390(), leerICM20948()) cuando detectan un error o
// timeout, no en cada vuelta de loop().
void recuperarBusI2C() {
  // TODO
}

// -----------------------------------------------------------------------
// SCD30 (REQUISITOS.md §3.4, §3.5, §4.3, §5)
// -----------------------------------------------------------------------

// setAutoSelfCalibration(false); calibración manual a valor conocido al
// aire libre (REQUISITOS.md §3.5).
void inicializarSCD30() {
  // TODO
}

// Lee co2_ppm y t_scd_C (REQUISITOS.md §4.3).
void leerSCD30() {
  // TODO
}

// Envía la presión medida con setAmbientPressure() (rango 700-1400 mbar,
// fuera de rango se envía el límite) y registra p_inviata_hPa
// (REQUISITOS.md §3.4).
void enviarPresionSCD30() {
  // TODO
}

// -----------------------------------------------------------------------
// MS8607 (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

void inicializarMS8607() {
  // TODO
}

// Lee p_hPa, t_ms8607_C, rh_ms8607 (REQUISITOS.md §4.3).
void leerMS8607() {
  // TODO
}

// -----------------------------------------------------------------------
// 2x MAX31865 / PT1000 — brazo exterior y tubo (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

// Configura RREF=4300.0, RNOMINAL=1000.0 y el modo de cableado
// (REQUISITOS.md §5).
void inicializarPT1000() {
  // TODO
}

// Lee t_arm_C, t_tubo_C y el fault byte del MAX31865 en err_arm, err_tubo
// (REQUISITOS.md §4.3).
void leerPT1000() {
  // TODO
}

// -----------------------------------------------------------------------
// SHT45 (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

void inicializarSHT45() {
  // TODO
}

// Lee rh_sht, t_sht_C (REQUISITOS.md §4.3).
void leerSHT45() {
  // TODO
}

// -----------------------------------------------------------------------
// 4x DS18B20 (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

// Identifica las cuatro sondas por ROM (caja, pilas, centro, SCD30) y
// activa resolución 12 bits con setWaitForConversion(false): nunca
// bloquear el loop 750 ms (REQUISITOS.md §5).
void inicializarDS18B20() {
  // TODO
}

// Lee t_cassa_C, t_pile_C, t_centro_C, t_scd_ds_C sin bloquear
// (REQUISITOS.md §4.3, §5).
void leerDS18B20() {
  // TODO
}

// -----------------------------------------------------------------------
// LTR390 (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

void inicializarLTR390() {
  // TODO
}

// Lee uva_raw, uv_gain, uv_res (conteos crudos, REQUISITOS.md §4.3).
void leerLTR390() {
  // TODO
}

// -----------------------------------------------------------------------
// ICM-20948 (REQUISITOS.md §4.3, §5, §7)
// -----------------------------------------------------------------------

// Dirección 0x69 (AD0 alto), ±16 g, 100 Hz (REQUISITOS.md §5).
void inicializarICM20948() {
  // TODO
}

// Lee ax, ay, az, gx, gy, gz, mx, my, mz desde la FIFO interna del
// ICM-20948 (que muestrea a 100 Hz por su cuenta): vacía todas las
// muestras presentes en la FIFO en cada pasada del tic de 10 ms, para no
// perder muestras si el tic de 1000 ms se retrasó (clock stretching del
// SCD30, GPS, flush de la SD). Cada muestra conserva su propio t_ms,
// para el buffer circular de la ventana IMU (REQUISITOS.md §4.3, §4.6, §7).
void leerICM20948() {
  // TODO
}

// -----------------------------------------------------------------------
// PMS5003 (REQUISITOS.md §3.6, §3.7, §4.3, §6)
// -----------------------------------------------------------------------

// Apaga el PMS5003: primero pone RX/TX de Serial1 en alta impedancia y
// recién después corta la masa por el MOSFET. El MOSFET (canal N) corta
// la masa, no la alimentación positiva: con la masa cortada, si TX
// quedara activo su reposo en alto inyectaría corriente (hasta 8 mA) por
// los diodos de protección del PMS y lo alimentaría a medias — parece
// apagado y no lo está, con riesgo de dejarlo en un estado indefinido, y
// en 4 h de vuelo son ~34 mAh (~4 % del balance de energía).
void pmsOff() {
  COMETA_PMS5003_SERIAL.end();
  pinMode(COMETA_PMS5003_RX_PIN, INPUT);  // alta impedancia: nada de corriente hacia el sensor
  pinMode(COMETA_PMS5003_TX_PIN, INPUT);
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, LOW);
}

// Enciende el PMS5003: primero el MOSFET, después Serial1 (con
// COMETA_PMS5003_MOSFET_SETTLE_MS de por medio para el asentamiento del
// regulador). Ese delay() bloquea el loop unos 50 ms: aceptable porque
// solo ocurre en los cambios de estado de controlarPMS5003(), no en
// cada vuelta. Las primeras COMETA_PMS_CALENTAMIENTO_S de lecturas tras
// esto se descartan en leerPMS5003(): el ventilador tarda en
// estabilizarse (REQUISITOS.md §6).
void pmsOn() {
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, HIGH);
  delay(COMETA_PMS5003_MOSFET_SETTLE_MS);
  COMETA_PMS5003_SERIAL.begin(COMETA_PMS5003_BAUD);
}

// Configura COMETA_PMS5003_MOSFET_PIN como salida y llama a pmsOff()
// para arrancar siempre en un estado conocido.
void inicializarPMS5003() {
  // TODO
}

// Apaga por encima de 5 km; por debajo, en subida, histéresis −15/−12 °C
// con T exterior (PT1000 brazo); en bajada tras el estallido, se
// reenciende por debajo de 5 km. Llama a pmsOn()/pmsOff() solo en los
// cambios de estado. pms_on se registra siempre (REQUISITOS.md §6).
void controlarPMS5003() {
  // TODO
}

// Lee pm1, pm25, pm10, n03, n05, n10; descarta las primeras
// COMETA_PMS_CALENTAMIENTO_S de cada encendido, el ventilador tarda en
// estabilizarse (REQUISITOS.md §4.3, §6).
void leerPMS5003() {
  // TODO
}

// -----------------------------------------------------------------------
// Geiger GGreg20 (REQUISITOS.md §4.3, §5)
// -----------------------------------------------------------------------

// La ISR solo incrementa un contador (REQUISITOS.md §5).
void inicializarGeiger() {
  // TODO
}

// Guarda los conteos por intervalo: cpi, dt_ms (factor de Fano,
// REQUISITOS.md §4.3, §5).
void leerGeiger() {
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
// Log SCI (REQUISITOS.md §2, §4.2, §4.3)
// -----------------------------------------------------------------------

// Los encabezados de SCI_nnn.CSV y de IMU_nnn.CSV (escribirEncabezadoSCI,
// escribirEncabezadoIMU) están en include/log_format.h, compartido con
// src/adalogger.

// Compone y escribe una fila de SCI_nnn.CSV a partir de las últimas
// lecturas (celda vacía = no hubo lectura en esa fila, REQUISITOS.md §4.2).
void escribirFilaSCI() {
  // TODO
}

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  // TODO
}

// Escribe en META_nnn.TXT: hash de Git, frecuencia de reloj, RREF/
// RNOMINAL, ROM de cada DS18B20, resultado Airborne, direcciones I²C
// detectadas y datos del disparo IMU si ocurrió (REQUISITOS.md §4.7).
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

  inicializarGPS();
  inicializarSCD30();
  inicializarMS8607();
  inicializarPT1000();
  inicializarSHT45();
  inicializarDS18B20();
  inicializarLTR390();
  inicializarICM20948();
  inicializarPMS5003();
  inicializarGeiger();

  escribirMETA();
}

void loop() {
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

    leerICM20948();
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

    leerGPS();
    leerMS8607();
    leerPT1000();
    leerSHT45();
    leerSCD30();
    enviarPresionSCD30();
    leerDS18B20();
    leerLTR390();
    leerPMS5003();
    leerGeiger();
    leerBateria();

    controlarPMS5003();
    calcularFlagsCalidad();

    escribirFilaSCI();
    flushLogSiCorresponde();
  }
}
