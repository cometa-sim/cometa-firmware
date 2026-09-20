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
#include <Adafruit_SleepyDog.h>

#include "config_adalogger.h"
#include "log_format.h"

// -----------------------------------------------------------------------
// Archivo de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoL2;

// Fila L2 en construcción durante el tic de 1000 ms (REQUISITOS.md
// §4.5). limpiarFilaL2() la vacía al principio de cada tic.
FilaL2 filaL2;

// -----------------------------------------------------------------------
// Watchdog (REQUISITOS.md §3.11): Adafruit_SleepyDog, timeout
// COMETA_WATCHDOG_TIMEOUT_S. Se alimenta (Watchdog.reset()) una sola vez
// por pasada de loop().
// -----------------------------------------------------------------------
void configurarWatchdog() {
  Watchdog.enable(COMETA_WATCHDOG_TIMEOUT_S * 1000);
}

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
// Verificación de configuración (valores [VERIFICAR] de
// config_adalogger.h todavía sin confirmar). Esta placa no tiene META:
// se informa por Serial.
// -----------------------------------------------------------------------

// Escribe "CONFIG INCOMPLETA: <nombre>" por Serial por cada centinela sin
// confirmar (los valores [VERIFICAR] no se inventan).
void reportarConfigIncompleta(const char *nombre) {
  Serial.print("CONFIG INCOMPLETA: ");
  Serial.println(nombre);
}

// Recorre las constantes marcadas [VERIFICAR] en config_adalogger.h y
// las informa por Serial antes de volar. Los demás centinelas ya se
// fijaron a valores decididos (REQUISITOS.md §2.1): solo queda el
// timeout I²C.
void verificarConfiguracion() {
  Serial.begin(115200);

  if (COMETA_I2C_TIMEOUT_MS == COMETA_MS_VERIFICAR) {
    reportarConfigIncompleta("I2C_TIMEOUT_MS");
  }
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
// (REQUISITOS.md §3.9, §4.5). Vuelca en f, solo si hay un fix nuevo.
void leerGPS(FilaL2 &f) {
  (void)f;
  // TODO
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Si SDA queda baja: 9 pulsos en SCL y reinicializar (REQUISITOS.md
// §3.3). Se llama desde leerGPS() cuando detecta un error o timeout, no
// en cada vuelta de loop().
void recuperarBusI2C() {
  // TODO
}

// -----------------------------------------------------------------------
// Batería (REQUISITOS.md §3.10, §4.5)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10). Vuelca en f, solo si hay un
// dato nuevo.
void leerBateria(FilaL2 &f) {
  (void)f;
  // TODO
}

// -----------------------------------------------------------------------
// Log L2 (REQUISITOS.md §2, §4.2, §4.5)
// -----------------------------------------------------------------------
//
// El encabezado (escribirEncabezadoL2), la struct FilaL2, limpiarFilaL2()
// y escribirFilaL2() están en include/log_format.h, compartido con
// src/teensy (REQUISITOS.md §1).

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  // TODO
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

void setup() {
  verificarConfiguracion();
  configurarWatchdog();

  // El watchdog ya está armado acá: hay que alimentarlo entre un paso
  // del arranque y el siguiente (REQUISITOS.md §3.11). Montar la SD,
  // buscar el primer nnn libre y configurar el GPS pueden sumar más de
  // COMETA_WATCHDOG_TIMEOUT_S sin que nada esté colgado, y la placa se
  // reiniciaría antes de llegar a loop(), una y otra vez.
  inicializarSD();
  Watchdog.reset();
  abrirArchivoL2();
  Watchdog.reset();
  inicializarGPS();
}

void loop() {
  Watchdog.reset();  // una sola vez por pasada de loop() (REQUISITOS.md §3.11)

  // Tick a 1 Hz (COMETA_TICK_L2_MS), sin delay() (REQUISITOS.md §4.1). Se
  // reprograma sumando el período, no fijándolo a "ahora", para no
  // acumular atraso; si se atrasó más de un período completo, se
  // realinea a "ahora" en vez de intentar recuperarlo.
  const unsigned long ahora = millis();
  static unsigned long ultimoTickL2 = 0;

  if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
    ultimoTickL2 += COMETA_TICK_L2_MS;
    if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
      ultimoTickL2 = ahora;
    }

    limpiarFilaL2(filaL2);
    filaL2.t_ms = ahora;

    leerGPS(filaL2);
    leerBateria(filaL2);

    escribirFilaL2(archivoL2, filaL2);
    flushLogSiCorresponde();
  }
}
