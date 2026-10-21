// COMETA · Teensy 4.1 — banco de prueba por USB
//
// Para probar los sensores desde el computador, con el Teensy alimentado
// por USB y sin la cadena de pilas. NO es el firmware de vuelo: no hay
// SD, ni META, ni watchdog, ni ventana IMU. Usa los MISMOS módulos de
// src/teensy/sensores/ y la misma fila FilaSCI de log_format.h, así que
// lo que se ve acá es lo que el firmware de vuelo pondría en el log.
//
// Qué hace:
//   - al arrancar y cada DIAGNOSTICO_CADA_MS, escanea el bus I²C y
//     compara con las direcciones esperadas de config_teensy.h (prueba 6
//     en chico), y lee el registro de configuración de los dos MAX31865;
//     se repite para no perderlo si el monitor se abre tarde, junto con
//     el bloqueo máximo de cada módulo en esos 10 s, contra el límite
//     COMETA_BLOQUEO_MAX_MS de REQUISITOS.md §1.2;
//   - inicia cada sensor con la misma regla de presencia/ausencia de
//     src/teensy/main.cpp (REQUISITOS.md §1.3): un sensor que falta queda
//     ausente y se reintenta cada COMETA_SENSOR_REINTENTO_MS, sin trabar
//     a los demás;
//   - cada COMETA_TICK_SCI_MS imprime por Serial qué sensores están
//     presentes y las celdas NO vacías de la fila SCI (i2c_recov queda
//     vacía: recuperarBusI2C() sigue TODO en el firmware de vuelo).
//
// Compilar y cargar:  pio run -e banco -t upload
// Mirar la salida:    pio device monitor -e banco

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "config_teensy.h"
#include "log_format.h"

#include "../teensy/sensores/ds18b20.h"
#include "../teensy/sensores/geiger.h"
#include "../teensy/sensores/gps.h"
#include "../teensy/sensores/icm20948.h"
#include "../teensy/sensores/ltr390.h"
#include "../teensy/sensores/max31865.h"
#include "../teensy/sensores/ms8607.h"
#include "../teensy/sensores/pms5003.h"
#include "../teensy/sensores/scd30.h"
#include "../teensy/sensores/sht45.h"

// -----------------------------------------------------------------------
// Escaneo I²C al arrancar
// -----------------------------------------------------------------------

struct DireccionEsperada {
  uint8_t direccion;
  const char *nombre;
};

const DireccionEsperada direccionesEsperadas[] = {
    {COMETA_MS8607_I2C_ADDR_HUMEDAD, "MS8607 (humedad)"},
    {COMETA_GPS_I2C_ADDR, "SAM-M8Q"},
    {COMETA_SHT45_I2C_ADDR, "SHT45"},
    {COMETA_LTR390_I2C_ADDR, "LTR390"},
    {COMETA_SCD30_I2C_ADDR, "SCD30"},
    {COMETA_ICM20948_I2C_ADDR, "ICM-20948"},
    {COMETA_MS8607_I2C_ADDR_PRESION_TEMP, "MS8607 (presion/temp)"},
};
const size_t NUM_ESPERADAS = sizeof(direccionesEsperadas) / sizeof(direccionesEsperadas[0]);

const char *nombreDireccion(uint8_t direccion) {
  for (size_t i = 0; i < NUM_ESPERADAS; i++) {
    if (direccionesEsperadas[i].direccion == direccion) {
      return direccionesEsperadas[i].nombre;
    }
  }
  return "(no esperada)";
}

bool respondeI2C(uint8_t direccion) {
  Wire.beginTransmission(direccion);
  return Wire.endTransmission() == 0;
}

void imprimirHex(uint8_t valor) {
  Serial.print("0x");
  if (valor < 0x10) {
    Serial.print('0');
  }
  Serial.print(valor, HEX);
}

void escanearI2C() {
  Serial.println("Escaneo I2C (SDA 18, SCL 19):");
  for (uint8_t direccion = 0x08; direccion < 0x78; direccion++) {
    if (respondeI2C(direccion)) {
      Serial.print("  ");
      imprimirHex(direccion);
      Serial.print("  ");
      Serial.println(nombreDireccion(direccion));
    }
  }
  for (size_t i = 0; i < NUM_ESPERADAS; i++) {
    if (!respondeI2C(direccionesEsperadas[i].direccion)) {
      Serial.print("  FALTA ");
      imprimirHex(direccionesEsperadas[i].direccion);
      Serial.print("  ");
      Serial.println(direccionesEsperadas[i].nombre);
    }
  }
  Serial.println();
}

// -----------------------------------------------------------------------
// Diagnóstico del MAX31865: registro de configuración leído por SPI
// -----------------------------------------------------------------------

// Sensores/max31865.cpp da el chip por presente solo si relee 0xD1 (bias,
// conversión continua, 3 hilos, 50 Hz). Si no, este valor dice por qué:
// 0x00 o 0xFF = no hay respuesta en SDO (cable, alimentación, SDI/SDO
// cruzados); otro valor = el chip contesta pero con otra configuración.
uint8_t leerConfigMAX31865(SPIClass &bus, uint8_t cs) {
  bus.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  bus.transfer(0x00);  // registro 0, lectura
  const uint8_t valor = bus.transfer(0xFF);
  digitalWrite(cs, HIGH);
  bus.endTransaction();
  return valor;
}

void diagnosticoMAX31865(SPIClass &bus, uint8_t cs, const char *nombre) {
  Serial.print("  ");
  Serial.print(nombre);
  Serial.print(" (CS ");
  Serial.print(cs);
  Serial.print("): config ");
  imprimirHex(leerConfigMAX31865(bus, cs));
  Serial.println(", esperado 0xD1 (0x00 o 0xFF = no contesta)");
}

const uint32_t DIAGNOSTICO_CADA_MS = 10000UL;

void imprimirBloqueos();

void imprimirDiagnostico() {
  escanearI2C();
  Serial.println("SPI:");
  diagnosticoMAX31865(SPI, COMETA_MAX31865_CS_ARM_PIN, "MAX31865 brazo");
  diagnosticoMAX31865(COMETA_MAX31865_TUBO_SPI, COMETA_MAX31865_CS_TUBO_PIN, "MAX31865 tubo");
  imprimirBloqueos();
  Serial.println();
}

// -----------------------------------------------------------------------
// Presencia/ausencia: la misma regla que src/teensy/main.cpp
// (REQUISITOS.md §1.3)
// -----------------------------------------------------------------------

struct EstadoSensor {
  const char *nombre;
  bool (*iniciar)();
  bool (*actualizar)(uint32_t);
  void (*llenarFila)(FilaSCI &);
  bool presente;
  uint32_t proximoReintentoMs;
  uint8_t fallosConsecutivos;
  uint32_t reinit;
};

uint32_t reinitTotal = 0;

EstadoSensor sensores[] = {
    {"SCD30", SensorSCD30::iniciar, SensorSCD30::actualizar,
     SensorSCD30::llenarFila, false, 0, 0, 0},
    {"MS8607", SensorMS8607::iniciar, SensorMS8607::actualizar,
     SensorMS8607::llenarFila, false, 0, 0, 0},
    {"SHT45", SensorSHT45::iniciar, SensorSHT45::actualizar,
     SensorSHT45::llenarFila, false, 0, 0, 0},
    {"LTR390", SensorLTR390::iniciar, SensorLTR390::actualizar,
     SensorLTR390::llenarFila, false, 0, 0, 0},
    {"MAX31865_BRAZO", SensorPT1000Brazo::iniciar,
     SensorPT1000Brazo::actualizar, SensorPT1000Brazo::llenarFila, false, 0,
     0, 0},
    {"MAX31865_TUBO", SensorPT1000Tubo::iniciar, SensorPT1000Tubo::actualizar,
     SensorPT1000Tubo::llenarFila, false, 0, 0, 0},
    {"DS18B20", SensorDS18B20::iniciar, SensorDS18B20::actualizar,
     SensorDS18B20::llenarFila, false, 0, 0, 0},
    {"PMS5003", SensorPMS5003::iniciar, SensorPMS5003::actualizar,
     SensorPMS5003::llenarFila, false, 0, 0, 0},
    {"GPS", SensorGPS::iniciar, SensorGPS::actualizar, SensorGPS::llenarFila,
     false, 0, 0, 0},
};
const size_t NUM_SENSORES = sizeof(sensores) / sizeof(sensores[0]);

EstadoSensor estadoICM20948 = {"ICM20948", SensorICM20948::iniciar,
                                SensorICM20948::actualizar,
                                SensorICM20948::llenarFila, false, 0, 0, 0};

// Bloqueo más largo de cada llamada a actualizarSensor() (actualizar() o
// el reintento de iniciar()) desde el último diagnóstico, en µs. Mismo
// orden que sensores[]; el ICM-20948 aparte.
uint32_t bloqueoMaxUs[NUM_SENSORES] = {};
uint32_t bloqueoMaxICMUs = 0;

void intentarIniciarSensor(EstadoSensor &s, uint32_t ahora) {
  s.reinit++;
  reinitTotal++;
  s.presente = s.iniciar();
  if (!s.presente) {
    s.proximoReintentoMs = ahora + COMETA_SENSOR_REINTENTO_MS;
  }
}

void actualizarSensor(EstadoSensor &s, uint32_t ahora) {
  if (!s.presente) {
    if ((int32_t)(ahora - s.proximoReintentoMs) < 0) {
      return;
    }
    intentarIniciarSensor(s, ahora);
    return;
  }
  if (s.actualizar(ahora)) {
    s.fallosConsecutivos = 0;
  } else {
    s.fallosConsecutivos++;
    if (s.fallosConsecutivos >= COMETA_SENSOR_MAX_FALLOS) {
      s.presente = false;
      s.fallosConsecutivos = 0;
      s.proximoReintentoMs = ahora + COMETA_SENSOR_REINTENTO_MS;
    }
  }
}

// Llama a actualizarSensor() y guarda en maxUs su duración si es la más
// larga hasta ahora.
void actualizarYMedir(EstadoSensor &s, uint32_t ahora, uint32_t &maxUs) {
  const uint32_t inicio = micros();
  actualizarSensor(s, ahora);
  const uint32_t duracion = micros() - inicio;
  if (duracion > maxUs) {
    maxUs = duracion;
  }
}

void imprimirBloqueo(const char *nombre, uint32_t &maxUs, uint32_t limiteUs) {
  Serial.print("  ");
  Serial.print(nombre);
  Serial.print(": ");
  Serial.print(maxUs / 1000.0, 1);
  Serial.print(" ms");
  if (maxUs > limiteUs) {
    Serial.print("  <-- EXCEDE");
  }
  Serial.println();
  maxUs = 0;
}

// Bloqueo máximo de cada módulo desde el último diagnóstico, y lo pone a
// cero. Límite: COMETA_BLOQUEO_MAX_MS en el tic de 1000 ms. El ICM-20948
// (tic de 10 ms) se muestra sin marcar: ahí no hay delay(), solo la
// lectura I²C de ~23 bytes, y su límite real lo da la FIFO (§7).
void imprimirBloqueos() {
  Serial.print("Bloqueo maximo por modulo (limite ");
  Serial.print(COMETA_BLOQUEO_MAX_MS);
  Serial.println(" ms, REQUISITOS.md 1.2):");
  for (size_t i = 0; i < NUM_SENSORES; i++) {
    imprimirBloqueo(sensores[i].nombre, bloqueoMaxUs[i], COMETA_BLOQUEO_MAX_MS * 1000UL);
  }
  imprimirBloqueo("ICM20948 (tic 10 ms)", bloqueoMaxICMUs, UINT32_MAX);

  // Muestras leídas de la FIFO del ICM-20948 desde el último diagnóstico:
  // a 100 Hz tienen que ser ~100 por segundo. Menos = se pierden; las
  // resincronizaciones son FIFOs descartadas por desalineadas o llenas.
  static uint32_t muestrasAntes = 0;
  static uint32_t msAntes = 0;
  const uint32_t muestras = SensorICM20948::muestrasTotales();
  const uint32_t ms = millis();
  if (ms != msAntes) {
    Serial.print("IMU (FIFO): ");
    Serial.print((muestras - muestrasAntes) * 1000.0 / (ms - msAntes), 1);
    Serial.print(" muestras/s (esperado ~100), resincronizaciones: ");
    Serial.println(SensorICM20948::resincronizaciones());
  }
  muestrasAntes = muestras;
  msAntes = ms;
}

// -----------------------------------------------------------------------
// Salida por Serial
// -----------------------------------------------------------------------

// Un renglón: primero los presentes, después los ausentes.
void imprimirEstados() {
  Serial.print("ok:");
  for (size_t i = 0; i < NUM_SENSORES; i++) {
    if (sensores[i].presente) {
      Serial.print(' ');
      Serial.print(sensores[i].nombre);
    }
  }
  if (estadoICM20948.presente) {
    Serial.print(' ');
    Serial.print(estadoICM20948.nombre);
  }
  Serial.print("  |  AUSENTES:");
  for (size_t i = 0; i < NUM_SENSORES; i++) {
    if (!sensores[i].presente) {
      Serial.print(' ');
      Serial.print(sensores[i].nombre);
    }
  }
  if (!estadoICM20948.presente) {
    Serial.print(' ');
    Serial.print(estadoICM20948.nombre);
  }
  Serial.println();
}

// Celdas de la fila, cuatro por renglón; solo las no vacías
// (REQUISITOS.md §4.2: vacía = no hubo lectura nueva en esta fila).
const uint8_t CAMPOS_POR_RENGLON = 4;
const uint8_t ANCHO_CAMPO = 24;
uint8_t camposEnRenglon = 0;
uint8_t anchoEscrito = 0;

void cerrarCampo() {
  camposEnRenglon++;
  if (camposEnRenglon >= CAMPOS_POR_RENGLON) {
    Serial.println();
    camposEnRenglon = 0;
  } else {
    // Al menos un espacio, aunque el campo llene el ancho (utc).
    Serial.print(' ');
    for (uint8_t i = anchoEscrito + 1; i < ANCHO_CAMPO; i++) {
      Serial.print(' ');
    }
  }
}

void imprimirCampo(const char *nombre, uint32_t valor, int decimales) {
  (void)decimales;
  anchoEscrito = Serial.print(nombre) + Serial.print('=') + Serial.print(valor);
  cerrarCampo();
}

void imprimirCampo(const char *nombre, const char *valor, int decimales) {
  (void)decimales;
  anchoEscrito = Serial.print(nombre) + Serial.print('=') + Serial.print(valor);
  cerrarCampo();
}

void imprimirCampo(const char *nombre, double valor, int decimales) {
  anchoEscrito = Serial.print(nombre) + Serial.print('=') + Serial.print(valor, decimales);
  cerrarCampo();
}

void imprimirCampo(const char *nombre, float valor, int decimales) {
  imprimirCampo(nombre, (double)valor, decimales);
}

#define BANCO_VACIO_U32(campo)  false
#define BANCO_VACIO_U32N(campo) ((campo) == 0)
#define BANCO_VACIO_UTC(campo)  ((campo)[0] == '\0')
#define BANCO_VACIO_LL(campo)   isnan(campo)
#define BANCO_VACIO_F(campo)    isnan(campo)
#define BANCO_CAMPO(nombre, tipo, decimales)  \
  if (!BANCO_VACIO_##tipo(fila.nombre)) {     \
    imprimirCampo(#nombre, fila.nombre, decimales); \
  }

void imprimirFila(const FilaSCI &fila) {
  camposEnRenglon = 0;
  COMETA_SCI_CAMPOS(BANCO_CAMPO)
  if (camposEnRenglon != 0) {
    Serial.println();
  }
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

FilaSCI filaSCI;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
  }
  Serial.println();
  Serial.println("==== COMETA · banco de prueba por USB ====");

  Wire.begin();  // 100 kHz, como en vuelo: el SCD30 no admite más
#if COMETA_GPS_EN_WIRE1
  COMETA_GPS_WIRE.begin();  // el GPS solo, en su propio bus
#endif

  for (size_t i = 0; i < NUM_SENSORES; i++) {
    intentarIniciarSensor(sensores[i], millis());
  }
  intentarIniciarSensor(estadoICM20948, millis());
  SensorGeiger::iniciar();

  // Después de iniciar(): los MAX31865 ya tienen el CS como salida y el
  // SPI arrancado por la librería.
  imprimirDiagnostico();

  Serial.println("Sensores ausentes se reintentan cada 30 s.");
  Serial.println("PMS5003: las primeras 30 s tras encenderse se descartan.");
  Serial.println();
}

void loop() {
  const unsigned long ahora = millis();
  static unsigned long ultimoTickIMU = 0;
  static unsigned long ultimoTickSCI = 0;

  if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
    ultimoTickIMU += COMETA_TICK_IMU_MS;
    if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
      ultimoTickIMU = ahora;
    }
    actualizarYMedir(estadoICM20948, ahora, bloqueoMaxICMUs);
  }

  if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
    ultimoTickSCI += COMETA_TICK_SCI_MS;
    if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
      ultimoTickSCI = ahora;
    }

    const uint32_t inicioPasada = millis();
    limpiarFilaSCI(filaSCI);
    filaSCI.t_ms = ahora;

    for (size_t i = 0; i < NUM_SENSORES; i++) {
      actualizarYMedir(sensores[i], ahora, bloqueoMaxUs[i]);
      if (sensores[i].presente) {
        sensores[i].llenarFila(filaSCI);
      }
    }
    if (estadoICM20948.presente) {
      estadoICM20948.llenarFila(filaSCI);
    }

    SensorGeiger::actualizar(ahora);
    SensorGeiger::llenarFila(filaSCI);

    SensorPMS5003::controlarEncendido(ahora, filaSCI.alt_m, filaSCI.t_arm_C);
    filaSCI.pms_on = SensorPMS5003::estaEncendido() ? 1 : 0;

    filaSCI.reinit = (float)reinitTotal;
    filaSCI.loop_ms = (float)(millis() - inicioPasada);

    Serial.print("---- t = ");
    Serial.print(ahora / 1000);
    Serial.println(" s ----");
    imprimirEstados();
    imprimirFila(filaSCI);
    Serial.println();

    static uint32_t ultimoDiagnosticoMs = 0;
    if (ahora - ultimoDiagnosticoMs >= DIAGNOSTICO_CADA_MS) {
      ultimoDiagnosticoMs = ahora;
      imprimirDiagnostico();
    }
  }
}
