// gps.cpp
//
// SAM-M8Q del Teensy (REQUISITOS.md §3.1, §3.9, §4.3, §5).
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): begin() en I²C, modo DYN_MODEL_AIRBORNE1g, lectura de la
// solución con getPVT() y conversión de unidades (1e-7 grados, mm).
// Cambios respecto del banco de prueba, para cumplir REQUISITOS.md:
//   - lat/lon en double (un float tiene ~7 cifras: unos 0,4 m de error,
//     y el log escribe 6 decimales).
//   - Se relee el modelo con getDynamicModel() para confirmar el modo
//     Airborne (§3.1): airborneConfirmado() se lo da a META.
//   - Sin setI2COutput(): en la librería v3 va por VALSET, que el M8 no
//     entiende (§3.1); la salida I²C de fábrica ya incluye UBX. Sin
//     saveConfiguration(): el modelo se envía en CADA arranque, así que
//     no hace falta guardarlo.
//   - getPVT() con espera acotada (COMETA_GPS_ESPERA_PVT_MS, §1.2): por
//     defecto la librería espera hasta 1100 ms.
//   - Se agregan vz_ms, vn_ms, ve_ms, pdop y utc (§4.3).
//   - Sin fix, la fila queda vacía (no lat = 0, lon = 0).
//
// ATENCIÓN con utc: se escribe SOLO con copiarUTC(f.utc, cadena) de
// log_format.h (REQUISITOS.md §4.2).

#include "gps.h"

#include <SparkFun_u-blox_GNSS_v3.h>
#include <Wire.h>

#include "config_teensy.h"

namespace SensorGPS {

namespace {

SFE_UBLOX_GNSS gps;

bool airborne = false;

// "Hubo un fix nuevo en esta pasada de actualizar()" (REQUISITOS.md
// §4.2): sin fix, llenarFila() no escribe nada.
bool datoNuevo = false;
double lat = NAN;
double lon = NAN;
float altM = NAN;
float vzMs = NAN;
float vnMs = NAN;
float veMs = NAN;
float sats = NAN;
float pdop = NAN;
float fix = NAN;
char utc[COMETA_UTC_LARGO] = "";

}  // namespace

bool iniciar() {
  if (!gps.begin(Wire, COMETA_GPS_I2C_ADDR)) {
    return false;
  }
  // Modo Airborne <1g: sin él, el receptor deja de dar fix por encima de
  // 12 km (REQUISITOS.md §3.1). Se envía en cada arranque y se relee.
  gps.setDynamicModel(DYN_MODEL_AIRBORNE1g);
  airborne = (gps.getDynamicModel() == DYN_MODEL_AIRBORNE1g);
  return true;
}

bool airborneConfirmado() { return airborne; }

bool actualizar(uint32_t ahora) {
  (void)ahora;
  datoNuevo = false;

  // false = el receptor no contestó dentro de la espera: cuenta como
  // lectura fallida para el gestor de ausencia (REQUISITOS.md §1.3).
  if (!gps.getPVT(COMETA_GPS_ESPERA_PVT_MS)) {
    return false;
  }

  // El receptor contestó. Sin fix 3D (fixType 3) o con fix pero sin
  // gnssFixOK, la posición no vale: celdas vacías, no es un fallo.
  const uint8_t tipoFix = gps.getFixType();
  fix = tipoFix;
  sats = gps.getSIV();
  if (tipoFix < 3 || !gps.getGnssFixOk()) {
    return true;
  }

  lat = gps.getLatitude() / 1e7;              // 1e-7 grados -> grados
  lon = gps.getLongitude() / 1e7;
  altM = gps.getAltitudeMSL() / 1000.0f;      // mm -> m, sobre el nivel del mar
  vzMs = -gps.getNedDownVel() / 1000.0f;      // mm/s hacia abajo -> m/s hacia arriba (§4.2)
  vnMs = gps.getNedNorthVel() / 1000.0f;
  veMs = gps.getNedEastVel() / 1000.0f;
  pdop = gps.getPDOP() / 100.0f;              // 0,01 -> adimensional

  if (gps.getTimeValid() && gps.getDateValid()) {
    char cadena[32];
    snprintf(cadena, sizeof(cadena), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             gps.getYear(), gps.getMonth(), gps.getDay(), gps.getHour(),
             gps.getMinute(), gps.getSecond());
    copiarUTC(utc, cadena);
  } else {
    copiarUTC(utc, "");
  }

  datoNuevo = true;
  return true;
}

void llenarFila(FilaSCI &f) {
  // fix y sats van siempre que el receptor contestó: dicen por qué no
  // hay posición. El resto, solo con un fix nuevo.
  f.fix = fix;
  f.sats = sats;
  fix = NAN;
  sats = NAN;
  if (!datoNuevo) {
    return;  // celda vacía: no hubo fix nuevo en esta fila (REQUISITOS.md §4.2)
  }
  f.lat = lat;
  f.lon = lon;
  f.alt_m = altM;
  f.vz_ms = vzMs;
  f.vn_ms = vnMs;
  f.ve_ms = veMs;
  f.pdop = pdop;
  copiarUTC(f.utc, utc);
}

}  // namespace SensorGPS
