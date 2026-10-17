// COMETA · Teensy 4.1 (nivel 3) — cadena científica completa
//
// Contrato del firmware (REQUISITOS.md §1.2, §1.3): cada sensor es un
// módulo en src/teensy/sensores/ con la interfaz común iniciar()/
// actualizar()/llenarFila(). main.cpp arma la fila SCI, recorre los
// módulos con un gestor genérico (reintentos y ausencia, REQUISITOS.md
// §1.3) y escribe el log. Los módulos de sensor están implementados;
// también la SD (SCI_nnn.CSV con flush cada 10 filas, META_nnn.TXT), el
// reloj, la recuperación I²C, la batería, los flags y la ventana IMU.

#include <Arduino.h>
#include <SD.h>
#include <Watchdog_t4.h>
#include <Wire.h>

#include "config_teensy.h"
#include "log_format.h"

#include "sensores/ds18b20.h"
#include "sensores/geiger.h"
#include "sensores/gps.h"
#include "sensores/icm20948.h"
#include "sensores/ltr390.h"
#include "sensores/max31865.h"
#include "sensores/ms8607.h"
#include "sensores/pms5003.h"
#include "sensores/scd30.h"
#include "sensores/sht45.h"

// -----------------------------------------------------------------------
// Archivos de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoSCI;
File archivoIMU;
File archivoMETA;

// nnn de esta corrida, el mismo para SCI, META e IMU; -1 = sin SD (la
// placa sigue midiendo, sin log).
int numeroCorrida = -1;

// Hash de Git del firmware para META (REQUISITOS.md §4.7). Lo pone el
// build si lo define con -DCOMETA_GIT_HASH=\"...\"; compilado desde un
// .zip no hay Git, y queda a la vista que no se sabe.
#ifndef COMETA_GIT_HASH
#define COMETA_GIT_HASH "desconocido"
#endif

// Fila SCI en construcción durante el tic de 1000 ms (REQUISITOS.md
// §4.3). limpiarFilaSCI() la vacía al principio de cada tic; cada
// módulo de sensor completa solo sus propios campos.
FilaSCI filaSCI;

// -----------------------------------------------------------------------
// Watchdog (REQUISITOS.md §3.11): WDT_T4, timeout COMETA_WATCHDOG_TIMEOUT_S.
// Se alimenta una sola vez por pasada de loop(), nunca dentro de un
// módulo de sensor.
// -----------------------------------------------------------------------
WDT_T4<WDT1> wdt;

void configurarWatchdog() {
  WDT_timings_t config = {};
  config.timeout = COMETA_WATCHDOG_TIMEOUT_S;
  wdt.begin(config);
}

// -----------------------------------------------------------------------
// Reloj y arranque
// -----------------------------------------------------------------------

// Del core de Teensy 4 (clockspeed.c): pone el reloj de la CPU en la
// frecuencia alcanzable más cercana y actualiza F_CPU_ACTUAL, que es lo
// que va a META como cpu_hz.
extern "C" uint32_t set_arm_clock(uint32_t frequency);

// Fija el reloj de la CPU a COMETA_CPU_CLOCK_HZ (REQUISITOS.md §3.8): a
// 150 MHz consume bastante menos que a los 600 de fábrica, y el trabajo
// del loop (sensores por I²C, SD) no depende de la CPU.
void configurarRelojCPU() {
  set_arm_clock(COMETA_CPU_CLOCK_HZ);
}

// "SCI_" + 7 + ".CSV" -> "SCI_007.CSV".
void nombreArchivo(char *destino, size_t largo, const char *prefijo, int n,
                   const char *extension) {
  snprintf(destino, largo, "%s%03d%s", prefijo, n, extension);
}

// Monta la SD (BUILTIN_SDCARD) y busca el primer número nnn libre para
// los archivos de esta corrida (REQUISITOS.md §4.1): libre = ni SCI_nnn
// ni META_nnn existen, para que los dos de una corrida lleven el mismo
// número. Sin SD, la placa sigue midiendo sin log y reintentarSD() lo
// vuelve a probar.
void inicializarSD() {
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.print("# SD: no monta (codigo de error 0x");
    Serial.print(SD.sdfs.sdErrorCode(), HEX);
    Serial.println("); se sigue sin log y se reintenta cada 10 s");
    return;
  }
  char nombre[16];
  for (int n = 0; n < COMETA_MAX_ARCHIVOS; n++) {
    nombreArchivo(nombre, sizeof(nombre), COMETA_SCI_PREFIJO, n, COMETA_SCI_EXT);
    if (SD.exists(nombre)) {
      continue;
    }
    nombreArchivo(nombre, sizeof(nombre), COMETA_META_PREFIJO, n, COMETA_META_EXT);
    if (SD.exists(nombre)) {
      continue;
    }
    numeroCorrida = n;
    return;
  }
  Serial.println("SD: no queda ningún nnn libre (000-999)");
}

// Crea <prefijo>nnn<extension> con el nnn de esta corrida. Un archivo
// nuevo en cada arranque: si la alimentación se corta, el anterior queda
// cerrado e intacto (REQUISITOS.md §4.1).
bool crearArchivo(File &archivo, const char *prefijo, const char *extension) {
  if (numeroCorrida < 0) {
    return false;
  }
  char nombre[16];
  nombreArchivo(nombre, sizeof(nombre), prefijo, numeroCorrida, extension);
  archivo = SD.open(nombre, FILE_WRITE);
  if (!archivo) {
    Serial.print("SD: no se pudo crear ");
    Serial.println(nombre);
    return false;
  }
  Serial.print("SD: escribiendo en ");
  Serial.println(nombre);
  return true;
}

// Crea SCI_nnn.CSV y escribe su encabezado (REQUISITOS.md §4.1, §4.3).
void abrirArchivoSCI() {
  if (!crearArchivo(archivoSCI, COMETA_SCI_PREFIJO, COMETA_SCI_EXT)) {
    return;
  }
  escribirEncabezadoSCI(archivoSCI);
  archivoSCI.flush();
}

// Crea META_nnn.TXT (datos que no cambian durante el vuelo,
// REQUISITOS.md §4.1, §4.7). Queda abierto: el disparo de la ventana IMU
// se agrega al final (§4.7).
void abrirArchivoMETA() {
  crearArchivo(archivoMETA, COMETA_META_PREFIJO, COMETA_META_EXT);
}

// -----------------------------------------------------------------------
// Verificación de configuración (valores [VERIFICAR] de config_teensy.h
// todavía sin confirmar)
// -----------------------------------------------------------------------

// Devuelve true si rom sigue siendo el centinela de ocho ceros: la sonda
// todavía no fue identificada por ROM (REQUISITOS.md §5).
bool romEsCentinela(const uint8_t rom[8]) {
  for (uint8_t i = 0; i < 8; i++) {
    if (rom[i] != 0x00) {
      return false;
    }
  }
  return true;
}

// Escribe "CONFIG INCOMPLETA: <nombre>" en META por cada centinela sin
// confirmar (los valores [VERIFICAR] no se inventan).
void reportarConfigIncompleta(const char *nombre) {
  if (!archivoMETA) {
    return;
  }
  archivoMETA.print("CONFIG INCOMPLETA: ");
  archivoMETA.println(nombre);
}

// Recorre las constantes marcadas [VERIFICAR] en config_teensy.h y
// registra en META cuáles siguen sin confirmar antes de volar. Los demás
// centinelas de config_teensy.h ya se fijaron a valores decididos
// (REQUISITOS.md §2.1, §5): solo quedan el timeout I²C y las cuatro ROM
// de los DS18B20.
void verificarConfiguracion() {
  {
    const uint8_t romCassa[8] = COMETA_DS18B20_ROM_CASSA;
    if (romEsCentinela(romCassa)) {
      reportarConfigIncompleta("DS18B20_ROM_CASSA");
    }
  }
  {
    const uint8_t romPile[8] = COMETA_DS18B20_ROM_PILE;
    if (romEsCentinela(romPile)) {
      reportarConfigIncompleta("DS18B20_ROM_PILE");
    }
  }
  {
    const uint8_t romCentro[8] = COMETA_DS18B20_ROM_CENTRO;
    if (romEsCentinela(romCentro)) {
      reportarConfigIncompleta("DS18B20_ROM_CENTRO");
    }
  }
  {
    const uint8_t romScd[8] = COMETA_DS18B20_ROM_SCD;
    if (romEsCentinela(romScd)) {
      reportarConfigIncompleta("DS18B20_ROM_SCD");
    }
  }
  if (COMETA_I2C_TIMEOUT_MS == COMETA_MS_VERIFICAR) {
    reportarConfigIncompleta("I2C_TIMEOUT_MS");
  }
}

// -----------------------------------------------------------------------
// Sensores que no responden (REQUISITOS.md §1.3)
//
// Lógica genérica, en un único lugar: cada módulo de sensor solo
// implementa iniciar()/actualizar()/llenarFila() (REQUISITOS.md §1.2).
// Si iniciar() falla, el sensor queda ausente (sus celdas quedan vacías
// y el loop sigue) y se reintenta cada COMETA_SENSOR_REINTENTO_MS.
// Si ya estaba presente, 5 lecturas fallidas consecutivas de
// actualizar() lo marcan ausente con la misma regla. reinit cuenta los
// intentos de iniciar() de todos los sensores.
// -----------------------------------------------------------------------

struct EstadoSensor {
  const char *nombre;
  // Está en el bus I²C: si pasa a ausente se intenta recuperar el bus
  // (REQUISITOS.md §3.3).
  bool i2c;
  bool (*iniciar)();
  bool (*actualizar)(uint32_t);
  void (*llenarFila)(FilaSCI &);
  bool presente;
  uint32_t proximoReintentoMs;
  uint8_t fallosConsecutivos;
  uint32_t reinit;
};

// Cuenta total de reintentos de iniciar(), de todos los sensores
// (columna `reinit` de SCI, REQUISITOS.md §1.3, §4.3).
uint32_t reinitTotal = 0;

// Sensores gobernados por la máquina de presencia/ausencia. Quedan
// fuera dos, por motivos distintos (REQUISITOS.md §1.3):
//   - ICM-20948: mismo EstadoSensor, pero se actualiza en el tic de
//     10 ms (ver loop()) porque su FIFO hay que vaciarla a 100 Hz.
//   - Geiger: no tiene con qué "no responder". Es un pin con una
//     interrupción, no un dispositivo de bus: iniciar() no puede fallar
//     y actualizar() no puede devolver false. Meterlo acá haría que un
//     `false` accidental lo marcara ausente y dejara de leerse el
//     contador, perdiendo cuentas en silencio. Se llama directo.
EstadoSensor sensores[] = {
    {"SCD30", true, SensorSCD30::iniciar, SensorSCD30::actualizar,
     SensorSCD30::llenarFila, false, 0, 0, 0},
    {"MS8607", true, SensorMS8607::iniciar, SensorMS8607::actualizar,
     SensorMS8607::llenarFila, false, 0, 0, 0},
    {"SHT45", true, SensorSHT45::iniciar, SensorSHT45::actualizar,
     SensorSHT45::llenarFila, false, 0, 0, 0},
    {"LTR390", true, SensorLTR390::iniciar, SensorLTR390::actualizar,
     SensorLTR390::llenarFila, false, 0, 0, 0},
    {"MAX31865_BRAZO", false, SensorPT1000Brazo::iniciar,
     SensorPT1000Brazo::actualizar, SensorPT1000Brazo::llenarFila, false, 0,
     0, 0},
    {"MAX31865_TUBO", false, SensorPT1000Tubo::iniciar, SensorPT1000Tubo::actualizar,
     SensorPT1000Tubo::llenarFila, false, 0, 0, 0},
    {"DS18B20", false, SensorDS18B20::iniciar, SensorDS18B20::actualizar,
     SensorDS18B20::llenarFila, false, 0, 0, 0},
    {"PMS5003", false, SensorPMS5003::iniciar, SensorPMS5003::actualizar,
     SensorPMS5003::llenarFila, false, 0, 0, 0},
    {"GPS", true, SensorGPS::iniciar, SensorGPS::actualizar, SensorGPS::llenarFila,
     false, 0, 0, 0},
};
const size_t COMETA_NUM_SENSORES = sizeof(sensores) / sizeof(sensores[0]);

// ICM-20948: mismo EstadoSensor, tic aparte (100 Hz).
EstadoSensor estadoICM20948 = {"ICM20948", true, SensorICM20948::iniciar,
                                SensorICM20948::actualizar,
                                SensorICM20948::llenarFila, false, 0, 0, 0};

void recuperarBusI2C();

// Un intento de iniciar(): el primero al arrancar y cada reintento
// posterior cuentan igual en reinit (REQUISITOS.md §1.3), para que
// escribirMETA() pueda listar los sensores ausentes desde el primer
// momento. Si falla, el próximo reintento se programa relativo a `ahora`
// (no a un valor absoluto: si no, a partir del primer reintento la
// comparación de abajo sería siempre cierta y se reintentaría en cada
// tic en vez de cada COMETA_SENSOR_REINTENTO_MS).
void intentarIniciarSensor(EstadoSensor &s, uint32_t ahora) {
  s.reinit++;
  reinitTotal++;
  s.presente = s.iniciar();
  if (!s.presente) {
    s.proximoReintentoMs = ahora + COMETA_SENSOR_REINTENTO_MS;
  } else if (s.reinit > 1) {
    Serial.print("# ");
    Serial.print(s.nombre);
    Serial.println(": presente de nuevo");
  }
}

// Reintenta iniciar() cada COMETA_SENSOR_REINTENTO_MS si el sensor está
// ausente; si está presente, llama a actualizar() y cuenta fallos
// consecutivos hasta COMETA_SENSOR_MAX_FALLOS (REQUISITOS.md §1.3).
void actualizarSensor(EstadoSensor &s, uint32_t ahora) {
  if (!s.presente) {
    if ((int32_t)(ahora - s.proximoReintentoMs) < 0) {
      return;  // todavía no toca reintentar
    }
    intentarIniciarSensor(s, ahora);  // mismo conteo de reinit que el primer intento
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
      Serial.print("# ");
      Serial.print(s.nombre);
      Serial.println(": ausente tras 5 lecturas fallidas");
      // Recién acá, no en cada lectura fallida: un fallo suelto no es un
      // bus colgado, y reinicializar Wire por cada uno convertía un error
      // aislado en una cascada (en el banco, todo el I2C muerto a los
      // ~50 s).
      if (s.i2c) {
        recuperarBusI2C();
      }
    }
  }
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Cuenta las recuperaciones del bus (columna i2c_recov, REQUISITOS.md
// §4.3): las veces que SDA estaba de verdad baja, no las llamadas.
uint32_t contadorRecuperacionesI2C = 0;

// Si SDA quedó baja (un esclavo a mitad de un byte, típico de un corte
// durante el clock stretching del SCD30): 9 pulsos en SCL para que
// termine de sacarlo y suelte la línea, y se reinicia Wire
// (REQUISITOS.md §3.3, §4.3). La llama el gestor cuando un sensor I²C
// pasa a ausente (5 lecturas fallidas seguidas), no en cada fallo. Para
// leer SDA el pin pasa a GPIO, así que Wire.begin() se llama siempre al
// final: es lo que le devuelve el pin al I²C. Wire.end() del core de
// Teensy 4 está vacía, por eso no se llama.
void recuperarBusI2C() {
  pinMode(COMETA_I2C_SDA_PIN, INPUT_PULLUP);
  Serial.print("# I2C: recuperacion, SDA ");
  Serial.println(digitalRead(COMETA_I2C_SDA_PIN) == LOW ? "BAJA" : "alta");
  if (digitalRead(COMETA_I2C_SDA_PIN) == LOW) {
    pinMode(COMETA_I2C_SCL_PIN, OUTPUT);
    for (int i = 0; i < COMETA_I2C_RECOVERY_PULSOS_SCL; i++) {
      digitalWrite(COMETA_I2C_SCL_PIN, LOW);
      delayMicroseconds(5);
      digitalWrite(COMETA_I2C_SCL_PIN, HIGH);
      delayMicroseconds(5);
    }
    contadorRecuperacionesI2C++;
  }
  Wire.begin();
}

// -----------------------------------------------------------------------
// Batería (REQUISITOS.md §3.10, §4.3)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10). Vuelca en f, solo si hay un
// dato nuevo: como los módulos de sensor, recibe la fila por referencia
// y toca solo sus propios campos (REQUISITOS.md §1.2).
// Sin el divisor conectado (en el banco, por USB) el pin queda al aire
// y el valor no significa nada.
void leerBateria(FilaSCI &f) {
  const int lectura = analogRead(COMETA_VBATT_ADC_PIN);
  f.v_batt = lectura * (COMETA_ADC_VREF / ((1 << COMETA_ADC_BITS) - 1)) *
             COMETA_VBATT_DIVISOR_FACTOR;
}

// -----------------------------------------------------------------------
// Flags de calidad (REQUISITOS.md §4.4)
// -----------------------------------------------------------------------

// Calcula q_pms, q_rh, q_arm, q_tubo, q_p a partir de los valores ya
// cargados en f: datos fuera de especificación se marcan, no se
// descartan (REQUISITOS.md §3.7, §4.4). Se llama al final del tic, con
// la fila ya completa, y recibe la fila por referencia como todo lo que
// escribe datos (REQUISITOS.md §1.2).
//
// Sin el valor del que depende (celda vacía), el flag también queda
// vacío: "no se sabe" no es "en especificación" (REQUISITOS.md §4.2).
float flagBajoMinimo(float valor, float minimo) {
  if (isnan(valor)) {
    return NAN;
  }
  return valor < minimo ? 1 : 0;
}

void calcularFlagsCalidad(FilaSCI &f) {
  f.q_pms = flagBajoMinimo(f.t_arm_C, COMETA_Q_PMS_TEMP_ARM_MIN_C);
  f.q_rh = flagBajoMinimo(f.t_sht_C, COMETA_Q_RH_TEMP_SHT_MIN_C);
  f.q_arm = flagBajoMinimo(f.t_arm_C, COMETA_Q_ARM_TEMP_MIN_C);
  f.q_tubo = flagBajoMinimo(f.t_tubo_C, COMETA_Q_TUBO_TEMP_MIN_C);
  f.q_p = flagBajoMinimo(f.p_hPa, COMETA_Q_P_MIN_HPA);
}

// -----------------------------------------------------------------------
// Ventana IMU alrededor del estallido (REQUISITOS.md §7)
// -----------------------------------------------------------------------

// Buffer circular en RAM, siempre activo, en binario (REQUISITOS.md §7):
// COMETA_IMU_BUFFER_SEGUNDOS a COMETA_IMU_TASA_HZ, ~66 kB. Va en DMAMEM
// (la segunda RAM del Teensy 4.1): no hace falta que sea la rápida.
using SensorICM20948::MuestraIMU;
const uint32_t CAPACIDAD_BUFFER_IMU = COMETA_IMU_BUFFER_SEGUNDOS * COMETA_IMU_TASA_HZ;
DMAMEM MuestraIMU bufferIMU[CAPACIDAD_BUFFER_IMU];

// Muestras metidas en el buffer desde el arranque. La muestra número n
// está en bufferIMU[n % CAPACIDAD_BUFFER_IMU] mientras no la haya pisado
// otra, es decir mientras n >= masViejaEnBufferIMU().
uint32_t totalBufferIMU = 0;

uint32_t masViejaEnBufferIMU() {
  return totalBufferIMU > CAPACIDAD_BUFFER_IMU ? totalBufferIMU - CAPACIDAD_BUFFER_IMU : 0;
}

// Una sola ventana, sin rearmado (§7): DESARMADA -> ARMADA -> GRABANDO
// -> TERMINADA, y de ahí no se vuelve.
enum EstadoVentanaIMU { VENTANA_DESARMADA, VENTANA_ARMADA, VENTANA_GRABANDO, VENTANA_TERMINADA };
EstadoVentanaIMU estadoVentana = VENTANA_DESARMADA;

uint32_t disparoMs = 0;
uint32_t proximaAEscribir = 0;     // número de la próxima muestra para IMU_nnn.CSV
uint32_t filasIMUEscritas = 0;
uint32_t muestrasIMUPerdidas = 0;  // pisadas en el buffer antes de llegar al archivo

// Caída libre en curso: |a| bajo el umbral, sin interrupción, desde
// inicioCaidaLibreMs.
bool enCaidaLibre = false;
uint32_t inicioCaidaLibreMs = 0;
uint8_t fixesBajando = 0;

// Último UTC y última altitud conocidos, para META en el disparo (§4.7):
// en la fila del disparo puede no haber fix.
char ultimaUTC[COMETA_UTC_LARGO] = "";
float ultimaAltitudM = NAN;

// Dispara la ventana en t (§7): la grabación empieza por la muestra más
// vieja del buffer que sea de los últimos COMETA_IMU_VENTANA_PRE_S, y
// guardarVentanaIMU() la va escribiendo de a poco desde el tic de 10 ms.
void dispararVentanaIMU(const char *criterio, uint32_t t) {
  estadoVentana = VENTANA_GRABANDO;
  disparoMs = t;
  Serial.print("# IMU: disparo por ");
  Serial.println(criterio);
  SensorPMS5003::marcarDescenso();  // en bajada, el PMS5003 vuelve bajo 5 km (§6)

  const uint32_t desdeMs = t - COMETA_IMU_VENTANA_PRE_S * 1000UL;
  const uint32_t masVieja = masViejaEnBufferIMU();
  proximaAEscribir = totalBufferIMU;
  while (proximaAEscribir > masVieja &&
         (int32_t)(bufferIMU[(proximaAEscribir - 1) % CAPACIDAD_BUFFER_IMU].t_ms - desdeMs) >= 0) {
    proximaAEscribir--;
  }

  if (crearArchivo(archivoIMU, COMETA_IMU_PREFIJO, COMETA_IMU_EXT)) {
    escribirEncabezadoIMU(archivoIMU);
  }
  if (archivoMETA) {
    archivoMETA.print("imu_disparo: criterio=");
    archivoMETA.print(criterio);
    archivoMETA.print(" t_ms=");
    archivoMETA.print(t);
    archivoMETA.print(" utc=");
    archivoMETA.print(ultimaUTC);
    archivoMETA.print(" alt_m=");
    if (!isnan(ultimaAltitudM)) {
      archivoMETA.print(ultimaAltitudM, 1);
    }
    archivoMETA.println();
    archivoMETA.flush();
  }
}

// Mete en el buffer circular las muestras que el ICM-20948 leyó de su
// FIFO en este tic, y con cada una evalúa la caída libre: |a| <
// COMETA_IMU_DISPARO_CAIDA_LIBRE_G sostenido COMETA_IMU_DISPARO_CAIDA_LIBRE_MS
// (§7). El disparo es el instante en que se cumple el criterio, como
// dice §7; el estallido fue COMETA_IMU_DISPARO_CAIDA_LIBRE_MS antes, que
// entra con holgura en los COMETA_IMU_VENTANA_PRE_S previos.
void actualizarBufferIMU() {
  const MuestraIMU *muestras;
  const size_t n = SensorICM20948::tomarMuestras(&muestras);
  for (size_t i = 0; i < n; i++) {
    const MuestraIMU &m = muestras[i];
    bufferIMU[totalBufferIMU % CAPACIDAD_BUFFER_IMU] = m;
    totalBufferIMU++;

    if (estadoVentana != VENTANA_ARMADA) {
      continue;
    }
    if (SensorICM20948::moduloAceleracionG(m) >= COMETA_IMU_DISPARO_CAIDA_LIBRE_G) {
      enCaidaLibre = false;
      continue;
    }
    if (!enCaidaLibre) {
      enCaidaLibre = true;
      inicioCaidaLibreMs = m.t_ms;
    } else if (m.t_ms - inicioCaidaLibreMs >= COMETA_IMU_DISPARO_CAIDA_LIBRE_MS) {
      dispararVentanaIMU("caida_libre", m.t_ms);
    }
  }
}

// En el tic de 1000 ms, con la fila SCI ya completa: armado (altitud GPS
// > COMETA_IMU_ARMADO_ALTITUD_M, o p_hPa < COMETA_IMU_ARMADO_PRESION_HPA
// si no hay fix) y el disparo de respaldo por GPS (vz_ms <
// COMETA_IMU_DISPARO_VZ_MS en COMETA_IMU_DISPARO_VZ_FIXES fixes
// seguidos). Una fila sin fix no es un fix: ni suma ni corta la cuenta.
void verificarDisparoIMU(const FilaSCI &f, uint32_t ahora) {
  if (f.utc[0] != '\0') {
    copiarUTC(ultimaUTC, f.utc);
  }
  if (!isnan(f.alt_m)) {
    ultimaAltitudM = f.alt_m;
  }

  if (estadoVentana == VENTANA_DESARMADA) {
    const bool porAltitud = !isnan(f.alt_m) && f.alt_m > COMETA_IMU_ARMADO_ALTITUD_M;
    const bool porPresion =
        isnan(f.alt_m) && !isnan(f.p_hPa) && f.p_hPa < COMETA_IMU_ARMADO_PRESION_HPA;
    if (porAltitud || porPresion) {
      estadoVentana = VENTANA_ARMADA;
      Serial.println("# IMU: ventana armada");
      if (archivoMETA) {
        archivoMETA.print("imu_armado: criterio=");
        archivoMETA.print(porAltitud ? "altitud" : "presion");
        archivoMETA.print(" t_ms=");
        archivoMETA.println(ahora);
        archivoMETA.flush();
      }
    }
    return;
  }

  if (estadoVentana != VENTANA_ARMADA || isnan(f.vz_ms)) {
    return;
  }
  fixesBajando = (f.vz_ms < COMETA_IMU_DISPARO_VZ_MS) ? fixesBajando + 1 : 0;
  if (fixesBajando >= COMETA_IMU_DISPARO_VZ_FIXES) {
    dispararVentanaIMU("gps_vz", ahora);
  }
}

void terminarVentanaIMU() {
  estadoVentana = VENTANA_TERMINADA;
  Serial.print("# IMU: ventana cerrada, filas=");
  Serial.println(filasIMUEscritas);
  if (archivoIMU) {
    archivoIMU.close();
  }
  if (archivoMETA) {
    archivoMETA.print("imu_ventana: filas=");
    archivoMETA.print(filasIMUEscritas);
    archivoMETA.print(" perdidas=");
    archivoMETA.print(muestrasIMUPerdidas);
    archivoMETA.print(" fifo_resincronizaciones=");
    archivoMETA.println(SensorICM20948::resincronizaciones());
    archivoMETA.flush();
  }
}

// Después del disparo, en cada tic de 10 ms: escribe en IMU_nnn.CSV hasta
// COMETA_IMU_FILAS_POR_TIC muestras del buffer, de la más vieja a la más
// nueva, hasta COMETA_IMU_VENTANA_POST_S después del disparo
// (REQUISITOS.md §4.6, §7). De a pocas para no bloquear el tic; como
// escribe más filas de las que entran, alcanza a los 20 s previos en
// unos segundos, mucho antes de que el buffer los pise.
void guardarVentanaIMU(uint32_t ahora) {
  if (estadoVentana != VENTANA_GRABANDO) {
    return;
  }
  const uint32_t finMs = disparoMs + COMETA_IMU_VENTANA_POST_S * 1000UL;

  const uint32_t masVieja = masViejaEnBufferIMU();
  if (proximaAEscribir < masVieja) {
    muestrasIMUPerdidas += masVieja - proximaAEscribir;
    proximaAEscribir = masVieja;
  }

  for (uint8_t i = 0; i < COMETA_IMU_FILAS_POR_TIC && proximaAEscribir < totalBufferIMU; i++) {
    const MuestraIMU &m = bufferIMU[proximaAEscribir % CAPACIDAD_BUFFER_IMU];
    if ((int32_t)(m.t_ms - finMs) > 0) {
      terminarVentanaIMU();
      return;
    }
    if (archivoIMU) {
      FilaIMU fila;
      SensorICM20948::aFilaIMU(m, fila);
      escribirFilaIMU(archivoIMU, fila);
      if (++filasIMUEscritas % COMETA_IMU_FLUSH_CADA_N_FILAS == 0) {
        archivoIMU.flush();
      }
    }
    proximaAEscribir++;
  }

  // Sin IMU no llegan muestras que pasen finMs: se cierra por reloj.
  if (proximaAEscribir == totalBufferIMU && (int32_t)(ahora - finMs) > 0) {
    terminarVentanaIMU();
  }
}

// Para el banco, por USB (líneas con '#', fuera del CSV): el código de
// error de la tarjeta cuando cambia, y mientras se graba la ventana IMU,
// cuántas filas lleva y cuántas faltan. Con la SD muda, es lo único que
// dice si el problema es la tarjeta o el programa.
void informarSDPorUSB() {
  // Si la placa se reinicia, la SD puede quedar trabada a mitad de una
  // escritura y no volver a montar: sin estas líneas, el reinicio no se
  // ve en ningún lado. Se repiten cada 10 s porque tras un reinicio el
  // monitor tarda unos segundos en reconectarse.
  static uint32_t pasadas = 0;
  if (pasadas++ % 10 == 0) {
    Serial.print("# arranque hace ");
    Serial.print(millis() / 1000);
    Serial.print(" s; SD ");
    Serial.print(numeroCorrida < 0 ? "NO montada" : "montada");
    Serial.print("; cpu_hz ");
    Serial.print(F_CPU_ACTUAL);
    Serial.println("; compilado " __DATE__ " " __TIME__);
    if (CrashReport) {
      Serial.print("# causa del reinicio anterior: ");
      Serial.print(CrashReport);
    }
  }
  if (numeroCorrida < 0) {
    return;
  }
  static uint8_t errorAnterior = 0;
  const uint8_t error = SD.sdfs.sdErrorCode();
  if (error != errorAnterior) {
    errorAnterior = error;
    Serial.print("# SD: codigo de error 0x");
    Serial.print(error, HEX);
    Serial.print(", dato 0x");
    Serial.println(SD.sdfs.sdErrorData(), HEX);
  }
  if (estadoVentana == VENTANA_GRABANDO) {
    Serial.print("# IMU: filas escritas ");
    Serial.print(filasIMUEscritas);
    Serial.print(", pendientes ");
    Serial.println(totalBufferIMU - proximaAEscribir);
  }
}

// -----------------------------------------------------------------------
// Log SCI y META
// -----------------------------------------------------------------------

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  static uint32_t filasSinFlush = 0;
  if (!archivoSCI) {
    return;
  }
  if (++filasSinFlush >= COMETA_FLUSH_CADA_N_MUESTRAS) {
    archivoSCI.flush();
    filasSinFlush = 0;
  }
}

void escribirHexMETA(uint8_t valor) {
  archivoMETA.print("0x");
  if (valor < 0x10) {
    archivoMETA.print('0');
  }
  archivoMETA.print(valor, HEX);
}

void escribirROMMETA(const char *posicion, const uint8_t rom[8]) {
  archivoMETA.print("ds18b20_");
  archivoMETA.print(posicion);
  archivoMETA.print(": ");
  for (uint8_t i = 0; i < 8; i++) {
    escribirHexMETA(rom[i]);
    archivoMETA.print(i < 7 ? " " : "\n");
  }
}

void escribirEstadoSensorMETA(const EstadoSensor &s) {
  archivoMETA.print("sensor: ");
  archivoMETA.print(s.nombre);
  archivoMETA.print(" presente=");
  archivoMETA.print(s.presente ? 1 : 0);
  archivoMETA.print(" reinit=");
  archivoMETA.println(s.reinit);
}

// Escribe en META_nnn.TXT: hash de Git, frecuencia de reloj, RREF/
// RNOMINAL, ROM de cada DS18B20, resultado Airborne, direcciones I²C
// detectadas, la causa del último reinicio si el core la expone (si no,
// [VERIFICAR], REQUISITOS.md §3.11), y datos del disparo IMU si ocurrió
// (REQUISITOS.md §4.7).
//
// Para cada sensor, una línea con su nombre, si está presente y su
// contador propio de intentos de iniciar() (EstadoSensor.reinit). La
// columna `reinit` de SCI es la suma de todos: sirve para ver que algo
// se está reiniciando, no cuál. El desglose por sensor es el que
// contesta esa pregunta al analizar el vuelo (REQUISITOS.md §1.3, §4.7).
void escribirMETA() {
  if (!archivoMETA) {
    return;
  }
  archivoMETA.print("firmware_git: ");
  archivoMETA.println(COMETA_GIT_HASH);
  // Compilado desde un ZIP no hay hash: la fecha de compilación es lo que
  // dice si en la placa quedó el firmware recién compilado o uno viejo.
  archivoMETA.print("firmware_compilado: ");
  archivoMETA.println(__DATE__ " " __TIME__);
  archivoMETA.print("cpu_hz: ");
  archivoMETA.println(F_CPU_ACTUAL);
  archivoMETA.print("max31865_rref_ohm: ");
  archivoMETA.println(COMETA_MAX31865_RREF, 1);
  archivoMETA.print("max31865_rnominal_ohm: ");
  archivoMETA.println(COMETA_MAX31865_RNOMINAL, 1);

  const uint8_t romCassa[8] = COMETA_DS18B20_ROM_CASSA;
  const uint8_t romPile[8] = COMETA_DS18B20_ROM_PILE;
  const uint8_t romCentro[8] = COMETA_DS18B20_ROM_CENTRO;
  const uint8_t romScd[8] = COMETA_DS18B20_ROM_SCD;
  escribirROMMETA("cassa", romCassa);
  escribirROMMETA("pile", romPile);
  escribirROMMETA("centro", romCentro);
  escribirROMMETA("scd", romScd);

  // Resultado del Airborne <1g en este arranque (§3.1). Sin GPS al
  // arrancar no hubo a quién mandárselo.
  archivoMETA.print("gps_airborne: ");
  const EstadoSensor *gps = nullptr;
  for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
    if (strcmp(sensores[i].nombre, "GPS") == 0) {
      gps = &sensores[i];
    }
  }
  if (gps == nullptr || !gps->presente) {
    archivoMETA.println("sin GPS al arrancar");
  } else {
    archivoMETA.println(SensorGPS::airborneConfirmado() ? "confirmado" : "NO confirmado");
  }

  // Direcciones I²C que contestan ahora (§4.7, prueba 6).
  archivoMETA.print("i2c:");
  for (uint8_t direccion = 0x08; direccion < 0x78; direccion++) {
    Wire.beginTransmission(direccion);
    if (Wire.endTransmission() == 0) {
      archivoMETA.print(' ');
      escribirHexMETA(direccion);
    }
  }
  archivoMETA.println();

  // Umbrales de la ventana IMU con los que se compiló (§7): dicen si lo
  // que corre es el firmware de vuelo o el de prueba (teensy41_prueba_imu).
  archivoMETA.print("imu_umbrales: armado_alt_m=");
  archivoMETA.print(COMETA_IMU_ARMADO_ALTITUD_M, 1);
  archivoMETA.print(" armado_p_hPa=");
  archivoMETA.print(COMETA_IMU_ARMADO_PRESION_HPA, 1);
  archivoMETA.print(" caida_g=");
  archivoMETA.print(COMETA_IMU_DISPARO_CAIDA_LIBRE_G, 2);
  archivoMETA.print(" caida_ms=");
  archivoMETA.print(COMETA_IMU_DISPARO_CAIDA_LIBRE_MS);
  archivoMETA.print(" vz_ms=");
  archivoMETA.print(COMETA_IMU_DISPARO_VZ_MS, 1);
  archivoMETA.print(" fixes=");
  archivoMETA.println(COMETA_IMU_DISPARO_VZ_FIXES);

  // Causa del último reinicio (REQUISITOS.md §3.11): el core de Teensy 4
  // guarda en CrashReport la falla (dirección, tipo) o el motivo del
  // reinicio (watchdog, tensión baja, ...) y lo conserva tras el reinicio.
  archivoMETA.println("causa_reinicio:");
  archivoMETA.print(CrashReport);

  for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
    escribirEstadoSensorMETA(sensores[i]);
  }
  escribirEstadoSensorMETA(estadoICM20948);
  archivoMETA.flush();
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

// Si la SD no montó al arrancar, cada COMETA_SD_REINTENTO_MS la vuelve a
// montar y, si lo logra, crea SCI y META de esta corrida en ese momento:
// se pierden solo las filas de antes, no toda la corrida.
void reintentarSD(uint32_t ahora) {
  static uint32_t ultimoIntentoMs = 0;
  if (numeroCorrida >= 0 || ahora - ultimoIntentoMs < COMETA_SD_REINTENTO_MS) {
    return;
  }
  ultimoIntentoMs = ahora;
  inicializarSD();
  if (numeroCorrida < 0) {
    return;
  }
  Serial.print("# SD: montada a los ");
  Serial.print(ahora / 1000);
  Serial.println(" s del arranque");
  abrirArchivoSCI();
  abrirArchivoMETA();
  escribirMETA();
}

void setup() {
  // El watchdog se arma lo primero de todo, antes de tocar nada: así
  // cubre también el arranque, que es donde más fácil es colgarse (una
  // SD que no monta, un sensor que estira el clock del I²C para
  // siempre). A cambio hay que alimentarlo DESPUÉS DE CADA PASO del
  // arranque (REQUISITOS.md §3.11): ningún paso suelto puede pasarse de
  // COMETA_WATCHDOG_TIMEOUT_S, pero la suma de todos sí — montar la SD,
  // buscar el primer nnn libre y once iniciar() con sus timeouts se
  // pasan de 8 s sin que nada esté roto. Sin estos feed la placa se
  // reiniciaría antes de llegar a loop(), y otra vez, y otra: ciclo de
  // reinicios infinito y ni una fila de log.
  configurarWatchdog();

  // Solo para mirar en el banco: sin esperar a que haya un monitor
  // abierto. En vuelo nadie escucha y no cuesta nada.
  Serial.begin(115200);

  configurarRelojCPU();
  analogReadResolution(COMETA_ADC_BITS);  // batería en A0 (REQUISITOS.md §3.10)
  wdt.feed();
  inicializarSD();
  wdt.feed();
  abrirArchivoSCI();
  escribirEncabezadoSCI(Serial);  // para leer en el banco las filas que siguen
  wdt.feed();
  abrirArchivoMETA();
  wdt.feed();

  verificarConfiguracion();
  wdt.feed();

  // Bus I²C (REQUISITOS.md §5: SDA 18, SCL 19). Las librerías de SparkFun
  // (GPS, SCD30, MS8607, ICM-20948) no lo arrancan solas. Queda a
  // 100 kHz, el valor por defecto: el SCD30 no admite más.
  Wire.begin();

  // millis() se relee por sensor, no una vez antes del bucle: el
  // arranque puede durar varios segundos si hay varios ausentes, y cada
  // uno tiene que esperar 30 s desde su propio intento.
  for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
    intentarIniciarSensor(sensores[i], millis());
    wdt.feed();
  }
  intentarIniciarSensor(estadoICM20948, millis());
  wdt.feed();

  // El Geiger no pasa por la máquina de presencia/ausencia: es un pin
  // con una interrupción, iniciar() no puede fallar (REQUISITOS.md §1.3).
  SensorGeiger::iniciar();
  wdt.feed();

  escribirMETA();
}

void loop() {
  wdt.feed();  // una sola vez por pasada de loop() (REQUISITOS.md §3.11)

  const unsigned long ahora = millis();
  static unsigned long ultimoTickIMU = 0;
  static unsigned long ultimoTickSCI = 0;

  // Tick a 100 Hz (COMETA_TICK_IMU_MS): IMU y ventana del estallido
  // (REQUISITOS.md §7). Se reprograma sumando el período, no fijándolo a
  // "ahora", para no acumular atraso; si se atrasó más de un período
  // completo, se realinea a "ahora" en vez de intentar recuperarlo.
  if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
    ultimoTickIMU += COMETA_TICK_IMU_MS;
    if (ahora - ultimoTickIMU >= COMETA_TICK_IMU_MS) {
      ultimoTickIMU = ahora;
    }

    actualizarSensor(estadoICM20948, ahora);
    actualizarBufferIMU();
    guardarVentanaIMU(ahora);
  }

  // Tick a 1 Hz (COMETA_TICK_SCI_MS): resto de sensores y log SCI
  // (REQUISITOS.md §4.1). Misma reprogramación por suma que el tick IMU.
  if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
    ultimoTickSCI += COMETA_TICK_SCI_MS;
    if (ahora - ultimoTickSCI >= COMETA_TICK_SCI_MS) {
      ultimoTickSCI = ahora;
    }

    // loop_ms mide esta pasada del tic de 1000 ms, de punta a punta
    // (REQUISITOS.md §4.3): cuánto tardaron todos los módulos más la
    // escritura en la SD. Es el número que dice si el presupuesto de
    // bloqueo de §1.2 se está respetando en vuelo; si se acerca a 1000
    // el tic se empieza a atrasar y hay que mirar qué módulo bloquea.
    const uint32_t inicioPasada = millis();

    limpiarFilaSCI(filaSCI);
    filaSCI.t_ms = ahora;

    for (size_t i = 0; i < COMETA_NUM_SENSORES; i++) {
      actualizarSensor(sensores[i], ahora);
      if (sensores[i].presente) {
        sensores[i].llenarFila(filaSCI);
      }
    }
    if (estadoICM20948.presente) {
      estadoICM20948.llenarFila(filaSCI);
    }

    // El Geiger va directo, sin máquina de presencia/ausencia: leer y
    // poner a cero el contador de la ISR no puede fallar
    // (REQUISITOS.md §1.3, §5).
    SensorGeiger::actualizar(ahora);
    SensorGeiger::llenarFila(filaSCI);

    // Armado y disparo de respaldo por GPS de la ventana IMU (§7), antes
    // del PMS5003: el disparo le avisa que empezó la bajada (§6).
    verificarDisparoIMU(filaSCI, ahora);

    // El PMS5003 no se enciende/apaga con el criterio genérico de
    // arriba: decide según altitud y T exterior, con histéresis
    // (REQUISITOS.md §6), no según si "responde".
    SensorPMS5003::controlarEncendido(ahora, filaSCI.alt_m, filaSCI.t_arm_C);
    // pms_on va en todas las filas, también con el sensor ausente: sin
    // eso no se distingue un cero de un sensor apagado (REQUISITOS.md §6).
    filaSCI.pms_on = SensorPMS5003::estaEncendido() ? 1 : 0;

    leerBateria(filaSCI);
    calcularFlagsCalidad(filaSCI);

    filaSCI.i2c_recov = (float)contadorRecuperacionesI2C;
    filaSCI.reinit = (float)reinitTotal;

    // loop_ms se cierra justo antes de escribir la fila: incluye todo el
    // trabajo de la pasada menos el escribirFilaSCI() que lo registra.
    filaSCI.loop_ms = (float)(millis() - inicioPasada);

    if (archivoSCI) {
      escribirFilaSCI(archivoSCI, filaSCI);
      flushLogSiCorresponde();
    }
    reintentarSD(ahora);
    // La misma fila por USB, si hay un monitor abierto (banco).
    if (Serial) {
      escribirFilaSCI(Serial, filaSCI);
      informarSDPorUSB();
    }
  }
}
