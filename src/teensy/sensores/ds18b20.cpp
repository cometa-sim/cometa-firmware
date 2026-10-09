// ds18b20.cpp
//
// 4x DS18B20 en un bus 1-Wire (pin 2): caja, pilas, centro del volumen y
// SCD30. REQUISITOS.md §1.2, §4.3, §5.
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): OneWire en COMETA_DS18B20_BUS_PIN y DallasTemperature.
// Cambios respecto del banco de prueba, para cumplir REQUISITOS.md:
//   - Cuatro sondas identificadas por ROM (config_teensy.h), no
//     getTempCByIndex(0): el orden "por índice" del bus no dice cuál es
//     cuál y puede cambiar si una sonda falla.
//   - Sin bloqueo (§1.2): requestTemperatures() con
//     setWaitForConversion(false) — el banco de prueba esperaba 750 ms
//     en cada vuelta. Ciclo de dos tics:
//       tic A: lee caja y pilas, que ya convirtieron;
//       tic B: lee centro y SCD30, y lanza la conversión siguiente.
//     Cada lectura por ROM tarda unos 10 ms en el bus, así que leer las
//     cuatro juntas pasaría de COMETA_BLOQUEO_MAX_MS; de a dos, no. Cada
//     sonda da un dato nuevo cada 2 s y las filas del medio van vacías
//     (§4.2).
//   - Sonda desconectada (DEVICE_DISCONNECTED_C = −127 °C): celda vacía.
//
// Mientras las ROM de config_teensy.h sigan en el centinela de ceros,
// iniciar() escribe por Serial las ROM que encuentra en el bus, para
// poder copiarlas en config_teensy.h (REQUISITOS.md §5).

#include "ds18b20.h"

#include <DallasTemperature.h>
#include <OneWire.h>

#include "config_teensy.h"

namespace SensorDS18B20 {

namespace {

OneWire bus(COMETA_DS18B20_BUS_PIN);
DallasTemperature sondas(&bus);

const uint8_t NUM_SONDAS = 4;
const uint8_t SONDAS_POR_PASADA = 2;

// Mismo orden que las columnas t_cassa_C, t_pile_C, t_centro_C,
// t_scd_ds_C (REQUISITOS.md §4.3).
uint8_t roms[NUM_SONDAS][8] = {
    COMETA_DS18B20_ROM_CASSA,
    COMETA_DS18B20_ROM_PILE,
    COMETA_DS18B20_ROM_CENTRO,
    COMETA_DS18B20_ROM_SCD,
};

bool presente[NUM_SONDAS] = {};
bool datoNuevo[NUM_SONDAS] = {};
float temperatura[NUM_SONDAS] = {NAN, NAN, NAN, NAN};

// Primera sonda a leer en la próxima pasada (0 o 2), y si ya hay una
// conversión lanzada desde la que leer.
uint8_t siguiente = 0;
bool conversionLanzada = false;
uint32_t inicioConversionMs = 0;

bool romEsCentinela(const uint8_t rom[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    if (rom[i] != 0x00) {
      return false;
    }
  }
  return true;
}

void imprimirROMsDelBus() {
  uint8_t rom[8];
  bus.reset_search();
  Serial.println("DS18B20: ROM encontradas en el bus (copiar en config_teensy.h):");
  while (bus.search(rom)) {
    Serial.print("  { ");
    for (uint8_t i = 0; i < 8; i++) {
      Serial.print("0x");
      if (rom[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(rom[i], HEX);
      Serial.print(i < 7 ? ", " : " }\n");
    }
  }
}

void lanzarConversion(uint32_t ahora) {
  sondas.requestTemperatures();  // vuelve enseguida: setWaitForConversion(false)
  inicioConversionMs = ahora;
  conversionLanzada = true;
}

}  // namespace

bool iniciar() {
  sondas.begin();
  sondas.setResolution(COMETA_DS18B20_RESOLUCION_BITS);
  sondas.setWaitForConversion(false);

  bool algunaCentinela = false;
  uint8_t encontradas = 0;
  for (uint8_t i = 0; i < NUM_SONDAS; i++) {
    algunaCentinela |= romEsCentinela(roms[i]);
    presente[i] = !romEsCentinela(roms[i]) && sondas.isConnected(roms[i]);
    encontradas += presente[i];
  }
  if (algunaCentinela) {
    imprimirROMsDelBus();
  }

  siguiente = 0;
  conversionLanzada = false;
  return encontradas > 0;
}

bool actualizar(uint32_t ahora) {
  for (uint8_t i = 0; i < NUM_SONDAS; i++) {
    datoNuevo[i] = false;
  }

  if (!conversionLanzada) {
    lanzarConversion(ahora);
    return true;
  }
  if (ahora - inicioConversionMs <
      (uint32_t)sondas.millisToWaitForConversion(COMETA_DS18B20_RESOLUCION_BITS)) {
    return true;  // todavía convirtiendo: no es un fallo
  }

  // Lee dos sondas de la conversión ya terminada.
  uint8_t leidas = 0;
  uint8_t esperadas = 0;
  for (uint8_t i = siguiente; i < siguiente + SONDAS_POR_PASADA; i++) {
    if (!presente[i]) {
      continue;
    }
    esperadas++;
    const float t = sondas.getTempC(roms[i]);
    if (t != DEVICE_DISCONNECTED_C) {
      temperatura[i] = t;
      datoNuevo[i] = true;
      leidas++;
    }
  }

  siguiente += SONDAS_POR_PASADA;
  if (siguiente >= NUM_SONDAS) {
    siguiente = 0;
    lanzarConversion(ahora);  // ya se leyeron las cuatro: conversión nueva
  }

  // Fallo solo si había sondas que leer y ninguna contestó.
  return esperadas == 0 || leidas > 0;
}

void llenarFila(FilaSCI &f) {
  // Cada sonda solo si trajo un dato nuevo (REQUISITOS.md §4.2).
  if (datoNuevo[0]) f.t_cassa_C = temperatura[0];
  if (datoNuevo[1]) f.t_pile_C = temperatura[1];
  if (datoNuevo[2]) f.t_centro_C = temperatura[2];
  if (datoNuevo[3]) f.t_scd_ds_C = temperatura[3];
}

}  // namespace SensorDS18B20
