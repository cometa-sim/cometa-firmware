// COMETA · Feather M0 Adalogger (nivel 2) — posición y altitud de respaldo
//
// Impalcatura del firmware: setup()/loop() llaman a funciones declaradas
// pero vacías. La única lógica implementada de verdad es la escritura
// del encabezado CSV (REQUISITOS.md §4).
//
// Esta placa es independiente de la del Teensy (REQUISITOS.md §2): no
// asume nada de la otra cadena. La única relación entre los dos logs es
// el UTC del GPS.

#include <Arduino.h>
#include <SD.h>

#include "config_adalogger.h"

// -----------------------------------------------------------------------
// Archivo de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoL2;

// Declaración adelantada: implementada más abajo junto con el log
// (REQUISITOS.md §4).
void escribirEncabezadoL2(File &archivo);

// -----------------------------------------------------------------------
// Arranque
// -----------------------------------------------------------------------

// Monta la SD (chip select COMETA_SD_CS_PIN) y busca el primer número
// nnn libre para el archivo de esta corrida (REQUISITOS.md §2, §4.1).
void inicializarSD() {
  // TODO
}

// Crea L2_nnn.CSV y escribe su encabezado (REQUISITOS.md §4.1, §4.5).
void abrirArchivoL2() {
  // TODO: crear el archivo con el primer nnn libre.
  escribirEncabezadoL2(archivoL2);
}

// -----------------------------------------------------------------------
// GPS — SAM-M8Q propio (REQUISITOS.md §3.1, §3.9, §4.5, §5)
// -----------------------------------------------------------------------

// Envía DYN_MODEL_AIRBORNE1g (CFG-NAV5 legacy), relee con
// getDynamicModel() y registra el resultado (REQUISITOS.md §3.1).
void inicializarGPS() {
  // TODO
}

// Lee posición, altitud, vz_ms, sats, fix y UTC del GPS
// (REQUISITOS.md §3.9, §4.5).
void leerGPS() {
  // TODO
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Si SDA queda baja: 9 pulsos en SCL y reinicializar
// (REQUISITOS.md §3.3).
void recuperarBusI2C() {
  // TODO
}

// -----------------------------------------------------------------------
// Batería (REQUISITOS.md §3.10, §4.5)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10).
void leerBateria() {
  // TODO
}

// -----------------------------------------------------------------------
// Log L2 (REQUISITOS.md §2, §4.2, §4.5)
// -----------------------------------------------------------------------

// Escribe el encabezado de L2_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.5.
void escribirEncabezadoL2(File &archivo) {
  archivo.println("t_ms,utc,lat,lon,alt_m,vz_ms,sats,fix,v_batt");
}

// Compone y escribe una fila de L2_nnn.CSV a partir de las últimas
// lecturas (celda vacía = no hubo lectura en esa fila, REQUISITOS.md §4.2).
void escribirFilaL2() {
  // TODO
}

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  // TODO
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

void setup() {
  inicializarSD();
  abrirArchivoL2();
  inicializarGPS();
}

void loop() {
  recuperarBusI2C();

  // Tick a 1 Hz (COMETA_TICK_L2_MS), sin delay() (REQUISITOS.md §4.1).
  const unsigned long ahora = millis();
  static unsigned long ultimoTickL2 = 0;

  if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
    ultimoTickL2 = ahora;

    leerGPS();
    leerBateria();

    escribirFilaL2();
    flushLogSiCorresponde();
  }
}
