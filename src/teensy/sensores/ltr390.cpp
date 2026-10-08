// ltr390.cpp
//
// LTR390 (UV-A, I²C 0x53). REQUISITOS.md §4.3, §5.
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): begin(), modo UVS, ganancia 3 y resolución de 18 bits.
// Cambios respecto del banco de prueba, para cumplir REQUISITOS.md:
//   - Se registran los conteos crudos (uva_raw) junto con la ganancia y
//     la resolución en uso (uv_gain, uv_res), no un "índice UV": la
//     conversión a índice depende de la ganancia, la resolución y la
//     ventana del sensor, y se hace en post-procesamiento (§4.3). El
//     factor 2300 del banco de prueba vale solo para ganancia 18 y
//     20 bits, no para la configuración elegida.
//   - Solo se lee cuando newDataAvailable() (sin dato nuevo, celda
//     vacía; §4.2).

#include "ltr390.h"

#include <Adafruit_LTR390.h>
#include <Wire.h>

#include "config_teensy.h"

namespace SensorLTR390 {

namespace {

Adafruit_LTR390 ltr;

bool datoNuevo = false;
float ultimoUVA = NAN;
float ganancia = NAN;
float resolucion = NAN;

// Factor de ganancia real de cada valor de ltr390_gain_t (hoja de datos).
float factorGanancia(ltr390_gain_t g) {
  switch (g) {
    case LTR390_GAIN_1:  return 1;
    case LTR390_GAIN_3:  return 3;
    case LTR390_GAIN_6:  return 6;
    case LTR390_GAIN_9:  return 9;
    case LTR390_GAIN_18: return 18;
  }
  return NAN;
}

// Bits de cada valor de ltr390_resolution_t.
float bitsResolucion(ltr390_resolution_t r) {
  switch (r) {
    case LTR390_RESOLUTION_20BIT: return 20;
    case LTR390_RESOLUTION_19BIT: return 19;
    case LTR390_RESOLUTION_18BIT: return 18;
    case LTR390_RESOLUTION_17BIT: return 17;
    case LTR390_RESOLUTION_16BIT: return 16;
    case LTR390_RESOLUTION_13BIT: return 13;
  }
  return NAN;
}

}  // namespace

bool iniciar() {
  if (!ltr.begin(&Wire)) {
    return false;
  }
  ltr.setMode(LTR390_MODE_UVS);
  ltr.setGain(LTR390_GAIN_3);
  ltr.setResolution(LTR390_RESOLUTION_18BIT);
  // Se relee lo que quedó configurado en el sensor, no lo que se pidió.
  ganancia = factorGanancia(ltr.getGain());
  resolucion = bitsResolucion(ltr.getResolution());
  return true;
}

bool actualizar(uint32_t ahora) {
  (void)ahora;
  datoNuevo = false;
  // A 18 bits el sensor convierte cada 100 ms: en un tic de 1 s siempre
  // hay dato nuevo. Si no lo hay, el sensor no está contestando.
  if (!ltr.newDataAvailable()) {
    return false;
  }
  ultimoUVA = ltr.readUVS();
  datoNuevo = true;
  return true;
}

void llenarFila(FilaSCI &f) {
  if (!datoNuevo) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.uva_raw = ultimoUVA;
  f.uv_gain = ganancia;
  f.uv_res = resolucion;
}

}  // namespace SensorLTR390
