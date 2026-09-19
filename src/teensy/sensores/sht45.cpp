// sht45.cpp
//
// Implementación completa, de ejemplo para el resto de los módulos de
// sensor (REQUISITOS.md §1.2).

#include "sht45.h"

#include <Adafruit_SHT4x.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

namespace SensorSHT45 {

namespace {

Adafruit_SHT4x sht4;

// "Hubo lectura válida en esta pasada de actualizar()": llenarFila()
// solo escribe si esto es true, para no repetir el último valor en una
// fila donde no hubo lectura nueva (REQUISITOS.md §4.2).
bool datoValido = false;
float ultimaHumedad = NAN;
float ultimaTemperatura = NAN;

}  // namespace

bool iniciar() {
  if (!sht4.begin(&Wire)) {
    return false;
  }
  // Alta precisión: conversión algo más lenta (unos pocos ms) pero más
  // resolución; se llama desde el tic de 1 s (COMETA_TICK_SCI_MS), sobra
  // tiempo de sobra.
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  // Sin calefactor: el termostato/calefacción no forma parte de la
  // configuración de vuelo (REQUISITOS.md §3).
  sht4.setHeater(SHT4X_NO_HEATER);
  return true;
}

bool actualizar(uint32_t ahora) {
  (void)ahora;
  sensors_event_t humedad;
  sensors_event_t temperatura;
  datoValido = sht4.getEvent(&humedad, &temperatura);
  if (datoValido) {
    ultimaHumedad = humedad.relative_humidity;
    ultimaTemperatura = temperatura.temperature;
  }
  return datoValido;
}

void llenarFila(FilaSCI &f) {
  if (!datoValido) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.rh_sht = ultimaHumedad;
  f.t_sht_C = ultimaTemperatura;
}

}  // namespace SensorSHT45
