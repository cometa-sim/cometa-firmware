// COMETA · Teensy 4.1 (nivel 3) — cadena científica completa
//
// Impalcatura del firmware: setup()/loop() llaman a funciones declaradas
// pero vacías, una por sensor. La única lógica implementada de verdad es
// la escritura del encabezado CSV (REQUISITOS.md §4).

#include <Arduino.h>
#include <SD.h>

#include "config_teensy.h"

// -----------------------------------------------------------------------
// Archivos de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoSCI;
File archivoIMU;
File archivoMETA;

// Declaraciones adelantadas: las funciones de encabezado se implementan
// más abajo, junto con el resto del log (REQUISITOS.md §4).
void escribirEncabezadoSCI(File &archivo);
void escribirEncabezadoIMU(File &archivo);

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

// Crea IMU_nnn.CSV y escribe su encabezado; no se escribe en la SD hasta
// el disparo de la ventana (REQUISITOS.md §4.1, §4.6, §7).
void abrirArchivoIMU() {
  // TODO: crear el archivo con el primer nnn libre.
  escribirEncabezadoIMU(archivoIMU);
}

// Crea META_nnn.TXT (datos que no cambian durante el vuelo,
// REQUISITOS.md §4.1, §4.7).
void abrirArchivoMETA() {
  // TODO
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
// recuperaciones en i2c_recov (REQUISITOS.md §3.3, §4.3).
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

// Lee ax, ay, az, gx, gy, gz, mx, my, mz a 100 Hz para el buffer circular
// de la ventana IMU (REQUISITOS.md §4.3, §4.6, §7).
void leerICM20948() {
  // TODO
}

// -----------------------------------------------------------------------
// PMS5003 (REQUISITOS.md §3.6, §3.7, §4.3, §6)
// -----------------------------------------------------------------------

void inicializarPMS5003() {
  // TODO
}

// Apaga por encima de 5 km; por debajo, en subida, histéresis ±15/−12 °C
// con T exterior (PT1000 brazo); en bajada tras el estallido, se
// reenciende por debajo de 5 km. pms_on se registra siempre
// (REQUISITOS.md §6).
void controlarPMS5003() {
  // TODO
}

// Lee pm1, pm25, pm10, n03, n05, n10 (REQUISITOS.md §4.3).
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

// Guarda la ventana (20 s antes, 90 s después del disparo) en IMU_nnn.CSV
// y registra criterio, t_ms, UTC y altitud en META (REQUISITOS.md §4.7, §7).
void guardarVentanaIMU() {
  // TODO
}

// -----------------------------------------------------------------------
// Log SCI (REQUISITOS.md §2, §4.2, §4.3)
// -----------------------------------------------------------------------

// Escribe el encabezado de SCI_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.3.
void escribirEncabezadoSCI(File &archivo) {
  archivo.println(
      "t_ms,utc,"
      "lat,lon,alt_m,vz_ms,vn_ms,ve_ms,sats,pdop,fix,"
      "p_hPa,t_ms8607_C,rh_ms8607,"
      "t_arm_C,t_tubo_C,err_arm,err_tubo,"
      "rh_sht,t_sht_C,"
      "co2_ppm,t_scd_C,p_inviata_hPa,"
      "t_cassa_C,t_pile_C,t_centro_C,t_scd_ds_C,"
      "uva_raw,uv_gain,uv_res,"
      "ax,ay,az,gx,gy,gz,mx,my,mz,"
      "pms_on,pm1,pm25,pm10,n03,n05,n10,"
      "cpi,dt_ms,"
      "q_pms,q_rh,q_arm,q_tubo,q_p,"
      "v_batt,i2c_recov,loop_ms");
}

// Escribe el encabezado de IMU_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.6.
void escribirEncabezadoIMU(File &archivo) {
  archivo.println("t_ms,ax,ay,az,gx,gy,gz,mx,my,mz");
}

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
  abrirArchivoIMU();
  abrirArchivoMETA();

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
  recuperarBusI2C();

  leerGPS();
  leerMS8607();
  leerPT1000();
  leerSHT45();
  leerSCD30();
  enviarPresionSCD30();
  leerDS18B20();
  leerLTR390();
  leerICM20948();
  leerPMS5003();
  leerGeiger();
  leerBateria();

  controlarPMS5003();
  calcularFlagsCalidad();

  actualizarBufferIMU();
  verificarDisparoIMU();
  guardarVentanaIMU();

  escribirFilaSCI();
  flushLogSiCorresponde();
}
