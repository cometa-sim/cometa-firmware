// pms5003.cpp
//
// CÓDIGO DE ARQUITECTURA — NO TOCAR (REQUISITOS.md §1.2): apagar() y
// encender() están implementadas de verdad. No son "lógica de sensor":
// son la secuencia exacta de pines que evita la fuga de corriente por TX
// descrita abajo (REQUISITOS.md §6), y el orden de las líneas importa.
//
// El resto (iniciar/actualizar/llenarFila/controlarEncendido) está
// implementado a partir del banco de prueba de los
// estudiantes (COMETA-Teensy41, main.cpp hasta 6ba266c): el parser de la
// trama de 32 bytes por Serial1 a 9600 baudios, que sincroniza con la
// cabecera 0x42 0x4D y descarta las tramas con checksum incorrecto, y
// las concentraciones en "ambiente atmosférico" (bytes 10–15).
// Cambios respecto del banco de prueba, para cumplir REQUISITOS.md:
//   - Se registran también pm1 y los conteos n03, n05, n10 (§4.3).
//   - Sin dato nuevo, celda vacía, no 0 (§4.2).
//   - El MOSFET no queda encendido fijo desde el arranque: lo decide
//     controlarEncendido() (§6), y tras cada encendido se descartan las
//     primeras COMETA_PMS_CALENTAMIENTO_S.
//   - Sin la espera de 3 s en el arranque: el parser no bloquea (§1.2).
//   - pms_on lo escribe main.cpp en cada fila (estaEncendido()), incluso
//     con el sensor ausente (§6).

#include "pms5003.h"

#include "config_teensy.h"

namespace SensorPMS5003 {

// Apaga el PMS5003: primero pone RX/TX de Serial1 en alta impedancia y
// recién después corta la masa por el MOSFET. El MOSFET (canal N) corta
// la masa, no la alimentación positiva: con la masa cortada, si TX
// quedara activo su reposo en alto inyectaría corriente (hasta 8 mA) por
// los diodos de protección del PMS y lo alimentaría a medias — parece
// apagado y no lo está, con riesgo de dejarlo en un estado indefinido, y
// en 4 h de vuelo son ~34 mAh (~4 % del balance de energía).
void apagar() {
  COMETA_PMS5003_SERIAL.end();
  pinMode(COMETA_PMS5003_RX_PIN, INPUT);  // alta impedancia: nada de corriente hacia el sensor
  pinMode(COMETA_PMS5003_TX_PIN, INPUT);
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, LOW);
}

// Enciende el PMS5003: primero el MOSFET, después Serial1 (con
// COMETA_PMS5003_MOSFET_SETTLE_MS de por medio para el asentamiento del
// regulador). Ese delay() bloquea el loop unos 50 ms: aceptable porque
// solo ocurre en los cambios de estado de controlarEncendido(), no en
// cada vuelta.
void encender() {
  digitalWrite(COMETA_PMS5003_MOSFET_PIN, HIGH);
  delay(COMETA_PMS5003_MOSFET_SETTLE_MS);
  COMETA_PMS5003_SERIAL.begin(COMETA_PMS5003_BAUD);
}

namespace {

const uint8_t LARGO_TRAMA = 32;

bool encendido = false;
uint32_t encendidoDesdeMs = 0;
uint32_t ultimaTramaMs = 0;
bool despuesDelEstallido = false;
float ultimaAltitudM = NAN;

// Parser: buffer de la trama en curso y posición.
uint8_t trama[LARGO_TRAMA];
uint8_t posicion = 0;

bool datoNuevo = false;
float pm1 = NAN, pm25 = NAN, pm10 = NAN;
float n03 = NAN, n05 = NAN, n10 = NAN;

uint16_t palabra(uint8_t i) { return ((uint16_t)trama[i] << 8) | trama[i + 1]; }

// Lee todo lo que haya en Serial1, sin esperar. Devuelve true si cerró
// al menos una trama con checksum correcto (queda la última).
bool leerTramas() {
  bool valida = false;
  while (COMETA_PMS5003_SERIAL.available() > 0) {
    const uint8_t b = COMETA_PMS5003_SERIAL.read();
    // Sincronización con la cabecera 0x42 0x4D.
    if (posicion == 0 && b != 0x42) {
      continue;
    }
    if (posicion == 1 && b != 0x4D) {
      posicion = (b == 0x42) ? 1 : 0;
      continue;
    }
    trama[posicion++] = b;
    if (posicion < LARGO_TRAMA) {
      continue;
    }
    posicion = 0;

    uint16_t suma = 0;
    for (uint8_t i = 0; i < LARGO_TRAMA - 2; i++) {
      suma += trama[i];
    }
    if (suma != palabra(30)) {
      continue;  // trama corrupta: se descarta
    }
    // Concentraciones "ambiente atmosférico", en µg/m³ (bytes 10–15),
    // y partículas por 0,1 L de diámetro > 0,3 / 0,5 / 1,0 µm (16–21).
    pm1 = palabra(10);
    pm25 = palabra(12);
    pm10 = palabra(14);
    n03 = palabra(16);
    n05 = palabra(18);
    n10 = palabra(20);
    valida = true;
  }
  return valida;
}

void encenderYMarcar(uint32_t ahora) {
  encender();
  encendido = true;
  encendidoDesdeMs = ahora;
  ultimaTramaMs = ahora;
  posicion = 0;
}

void apagarYMarcar() {
  apagar();
  encendido = false;
  posicion = 0;
}

}  // namespace

bool iniciar() {
  // Arranca apagado, en un estado conocido. En un reintento del gestor
  // (sensor colgado, §1.3) esto le corta la masa: controlarEncendido()
  // lo vuelve a encender en el tic siguiente, con calentamiento nuevo.
  pinMode(COMETA_PMS5003_MOSFET_PIN, OUTPUT);
  apagarYMarcar();
  return true;
}

bool actualizar(uint32_t ahora) {
  datoNuevo = false;
  if (!encendido) {
    return true;  // apagado a propósito: no es un fallo
  }
  const bool hayTrama = leerTramas();
  if (hayTrama) {
    ultimaTramaMs = ahora;
  }
  if (ahora - encendidoDesdeMs < COMETA_PMS_CALENTAMIENTO_S * 1000UL) {
    return true;  // ventilador todavía estabilizándose: datos descartados (§6)
  }
  datoNuevo = hayTrama;
  return (ahora - ultimaTramaMs) < COMETA_PMS_SIN_TRAMA_MAX_MS;
}

void llenarFila(FilaSCI &f) {
  if (!datoNuevo) {
    return;  // celda vacía: no hubo trama nueva válida (REQUISITOS.md §4.2)
  }
  f.pm1 = pm1;
  f.pm25 = pm25;
  f.pm10 = pm10;
  f.n03 = n03;
  f.n05 = n05;
  f.n10 = n10;
}

bool estaEncendido() { return encendido; }

void marcarDescenso() { despuesDelEstallido = true; }

void controlarEncendido(uint32_t ahora, float altitud_m, float temperatura_C) {
  // Sin fix nuevo en esta fila, vale la última altitud conocida; sin
  // ninguna todavía (en tierra, antes del primer fix), se toma por
  // debajo del techo.
  if (!isnan(altitud_m)) {
    ultimaAltitudM = altitud_m;
  }
  const bool sobreTecho =
      !isnan(ultimaAltitudM) && ultimaAltitudM > COMETA_PMS_ALTITUD_TECHO_M;

  bool debeEstar;
  if (sobreTecho) {
    debeEstar = false;  // siempre apagado por encima de 5 km (§6)
  } else if (despuesDelEstallido || isnan(temperatura_C)) {
    debeEstar = true;   // en bajada, o sin T válida: decide solo el techo
  } else if (encendido) {
    debeEstar = temperatura_C >= COMETA_PMS_APAGAR_BAJO_C;     // histéresis
  } else {
    debeEstar = temperatura_C > COMETA_PMS_ENCENDER_SOBRE_C;
  }

  // Solo en los cambios de estado: encender() bloquea ~50 ms.
  if (debeEstar && !encendido) {
    encenderYMarcar(ahora);
  } else if (!debeEstar && encendido) {
    apagarYMarcar();
  }
}

}  // namespace SensorPMS5003
