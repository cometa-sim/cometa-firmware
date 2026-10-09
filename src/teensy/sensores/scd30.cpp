// scd30.cpp
//
// SCD30 (CO₂, I²C 0x61). REQUISITOS.md §3.4, §3.5, §4.3, §5.
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): begin(), dataAvailable() y la lectura de CO₂ y
// temperatura. Cambios respecto del banco de prueba, para cumplir
// REQUISITOS.md:
//   - Sin dato nuevo, la celda queda vacía, no en 0 ppm (§4.2): el SCD30
//     actualiza cada 2 s, así que una fila de cada dos va sin CO₂.
//   - Autocalibración apagada explícitamente (§3.5): el SCD30 la guarda
//     en su memoria, begin() solo no alcanza para apagarla.
//   - Compensación de presión con setAmbientPressure() (§3.4), con el
//     valor enviado en p_inviata_hPa.
//   - Sin Wire.setClock(400000): el SCD30 admite como máximo 100 kHz.

#include "scd30.h"

#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>

#include "config_teensy.h"
#include "ms8607.h"

namespace SensorSCD30 {

namespace {

SCD30 scd30;

bool datoNuevo = false;
float ultimoCO2 = NAN;
float ultimaTemperatura = NAN;
uint32_t ultimoDatoMs = 0;

// Último valor mandado con setAmbientPressure(), en mbar; 0 = todavía
// ninguno. Va en p_inviata_hPa en cada fila mientras el sensor esté
// presente: es la compensación que el sensor está aplicando.
uint16_t presionEnviada = 0;

// Manda la presión del MS8607 al SCD30, recortada al rango que acepta
// (700–1400 mbar, REQUISITOS.md §3.4): fuera de rango se manda el límite
// y la corrección se hace en post-procesamiento.
void enviarPresion() {
  const float presion = SensorMS8607::ultimaPresionHPa();
  if (isnan(presion)) {
    return;
  }
  long mbar = lroundf(presion);
  if (mbar < COMETA_SCD30_PRESION_MIN_MBAR) {
    mbar = COMETA_SCD30_PRESION_MIN_MBAR;
  }
  if (mbar > COMETA_SCD30_PRESION_MAX_MBAR) {
    mbar = COMETA_SCD30_PRESION_MAX_MBAR;
  }
  if (presionEnviada != 0 &&
      labs(mbar - (long)presionEnviada) < COMETA_SCD30_PRESION_DELTA_MBAR) {
    return;
  }
  if (scd30.setAmbientPressure((uint16_t)mbar)) {
    presionEnviada = (uint16_t)mbar;
  }
}

}  // namespace

bool iniciar() {
  if (!scd30.begin(Wire, COMETA_SCD30_AUTO_SELF_CALIBRATION)) {
    return false;
  }
  scd30.setAutoSelfCalibration(COMETA_SCD30_AUTO_SELF_CALIBRATION);
  presionEnviada = 0;
  ultimoDatoMs = millis();
  return true;
}

bool actualizar(uint32_t ahora) {
  datoNuevo = false;
  enviarPresion();

  if (scd30.dataAvailable() && scd30.readMeasurement()) {
    ultimoCO2 = scd30.getCO2();
    ultimaTemperatura = scd30.getTemperature();
    ultimoDatoMs = ahora;
    datoNuevo = true;
    return true;
  }

  // dataAvailable() no distingue "todavía no" de "no contesta": se
  // cuenta como fallo solo si hace demasiado que no llega nada.
  return (ahora - ultimoDatoMs) < COMETA_SCD30_SIN_DATO_MAX_MS;
}

void llenarFila(FilaSCI &f) {
  if (presionEnviada != 0) {
    f.p_inviata_hPa = presionEnviada;
  }
  if (!datoNuevo) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.co2_ppm = ultimoCO2;
  f.t_scd_C = ultimaTemperatura;
}

}  // namespace SensorSCD30
