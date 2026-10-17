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
//   - Librería u-blox GNSS v2, no v3: la v3 habla solo la interfaz de
//     configuración nueva (VALGET/VALSET), que el M8 no entiende. Con
//     ella begin() falla siempre (isConnected() pregunta por VALGET) y
//     setDynamicModel() no llega nunca (§3.1). La v2 usa los mensajes
//     legacy: CFG-PRT, CFG-NAV5, CFG-MSG. Se vio en el banco: 0x42
//     contestaba en el escaneo I²C y el GPS seguía ausente.
//   - setI2COutput(COM_TYPE_UBX): por I²C solo UBX, sin NMEA que leer y
//     descartar en cada tic. Sin saveConfiguration(): esto y el modelo
//     se envían en CADA arranque, así que no hace falta guardarlos.
//   - Sin bloqueo (§1.2): setAutoPVT(true) y el receptor manda NAV-PVT
//     solo, una vez por solución; getPVT() lee lo que haya llegado y
//     vuelve enseguida. Pedirlo con un poll obligaba a esperar la
//     respuesta en cada tic. Un tic sin NAV-PVT nuevo no es un fallo;
//     sí lo es pasar COMETA_GPS_SIN_PVT_MAX_MS sin ninguno (§1.3).
//   - Se agregan vz_ms, vn_ms, ve_ms, pdop y utc (§4.3).
//   - Sin fix, la fila queda vacía (no lat = 0, lon = 0).
//
// ATENCIÓN con utc: se escribe SOLO con copiarUTC(f.utc, cadena) de
// log_format.h (REQUISITOS.md §4.2).

#include "gps.h"

#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
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

// Último NAV-PVT recibido: sin ninguno durante COMETA_GPS_SIN_PVT_MAX_MS,
// lectura fallida (REQUISITOS.md §1.3).
uint32_t ultimoPVTMs = 0;

}  // namespace

bool iniciar() {
  if (!gps.begin(COMETA_GPS_WIRE, COMETA_GPS_I2C_ADDR)) {
    return false;
  }
  gps.setI2COutput(COM_TYPE_UBX);
  // Modo Airborne <1g: sin él, el receptor deja de dar fix por encima de
  // 12 km (REQUISITOS.md §3.1). Se envía en cada arranque y se relee.
  gps.setDynamicModel(DYN_MODEL_AIRBORNE1g);
  airborne = (gps.getDynamicModel() == DYN_MODEL_AIRBORNE1g);
  if (!gps.setAutoPVT(true)) {
    return false;  // sin NAV-PVT automático getPVT() no traería nada
  }
  ultimoPVTMs = millis();
  return true;
}

bool airborneConfirmado() { return airborne; }

bool actualizar(uint32_t ahora) {
  datoNuevo = false;

  // false = todavía no llegó un NAV-PVT nuevo. Normal en algún tic (el
  // receptor y el tic van a 1 Hz cada uno, sin sincronía); fallo recién
  // si pasa COMETA_GPS_SIN_PVT_MAX_MS sin ninguno (REQUISITOS.md §1.3).
  if (!gps.getPVT()) {
    return (ahora - ultimoPVTMs) < COMETA_GPS_SIN_PVT_MAX_MS;
  }
  ultimoPVTMs = ahora;

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
