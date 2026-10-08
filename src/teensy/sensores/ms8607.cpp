// ms8607.cpp
//
// MS8607 (presión, temperatura, humedad; I²C 0x76 + 0x40). REQUISITOS.md
// §4.3, §5.
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp), que lo leía con la librería de Adafruit. Acá la librería
// fijada en platformio.ini es la de SparkFun (SparkFun PHT MS8607): la
// lectura es la misma, cambian los nombres de las funciones.
//
// Presupuesto de bloqueo (REQUISITOS.md §1.2): la librería espera la
// conversión con delay(). Con la resolución máxima (OSR 8192 en presión
// y temperatura, 12 bits en humedad) son unos 18 + 18 + 16 ms, más que
// los 20 ms permitidos. Con OSR 2048 y humedad a 10 bits son unos
// 5 + 5 + 5 ms, y la resolución sigue sobrando: 0,036 hPa en presión
// (el fondo de escala que importa es 10 hPa, §5) y 0,08 %RH en humedad.

#include "ms8607.h"

#include <SparkFun_PHT_MS8607_Arduino_Library.h>
#include <Wire.h>

#include "config_teensy.h"

namespace SensorMS8607 {

namespace {

MS8607 ms8607;

// "Hubo lectura válida en esta pasada de actualizar()" (REQUISITOS.md
// §4.2).
bool datoValido = false;
float ultimaPresion = NAN;
float ultimaTemperatura = NAN;
float ultimaHumedad = NAN;

}  // namespace

bool iniciar() {
  if (!ms8607.begin(Wire)) {
    return false;
  }
  ms8607.set_pressure_resolution(MS8607_pressure_resolution_osr_2048);
  ms8607.set_humidity_resolution(MS8607_humidity_resolution_10b);
  return true;
}

bool actualizar(uint32_t ahora) {
  (void)ahora;
  float temperatura;
  float presion;
  float humedad;
  datoValido = (ms8607.read_temperature_pressure_humidity(
                    &temperatura, &presion, &humedad) == MS8607_status_ok);
  if (datoValido) {
    ultimaTemperatura = temperatura;
    ultimaPresion = presion;  // la librería ya la da en hPa (= mbar)
    ultimaHumedad = humedad;
  }
  return datoValido;
}

void llenarFila(FilaSCI &f) {
  if (!datoValido) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.p_hPa = ultimaPresion;
  f.t_ms8607_C = ultimaTemperatura;
  f.rh_ms8607 = ultimaHumedad;
}

float ultimaPresionHPa() { return datoValido ? ultimaPresion : NAN; }

}  // namespace SensorMS8607
