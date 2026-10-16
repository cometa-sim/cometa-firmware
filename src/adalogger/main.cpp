// COMETA · Feather M0 Adalogger (nivel 2) — posición y altitud de respaldo
//
// GPS SAM-M8Q por I²C -> fila L2 en la SD (L2_nnn.CSV) cada 1 s ->
// trama de radio por el E220 cada COMETA_RADIO_PERIODO_MS
// (REQUISITOS.md §2, §2.2, §4.5). Mensajes de diagnóstico por Serial:
// esta placa no tiene META.
//
// Esta placa es independiente de la del Teensy (REQUISITOS.md §2): no
// asume nada de la otra cadena. La única relación entre los dos logs es
// el UTC del GPS.

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_SleepyDog.h>
// Librería v2, no v3: la v3 solo habla VALGET/VALSET, que el M8 no
// entiende (begin() falla siempre). La v2 usa CFG-NAV5 legacy (§3.1).
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

#include "config_adalogger.h"
#include "log_format.h"

// -----------------------------------------------------------------------
// Archivo de log (REQUISITOS.md §4.1)
// -----------------------------------------------------------------------
File archivoL2;
char nombreL2[16] = "";  // "L2_nnn.CSV"; vacío si la SD no montó

// Fila L2 en construcción durante el tic de 1000 ms (REQUISITOS.md
// §4.5). limpiarFilaL2() la vacía al principio de cada tic.
FilaL2 filaL2;

// -----------------------------------------------------------------------
// Watchdog (REQUISITOS.md §3.11): Adafruit_SleepyDog, timeout
// COMETA_WATCHDOG_TIMEOUT_S. Se alimenta (Watchdog.reset()) una sola vez
// por pasada de loop(), y entre un paso y el siguiente del arranque.
// -----------------------------------------------------------------------

// Los periodos del WDT del SAMD21 están cuantizados (potencias de dos de
// ciclos del reloj de 1,024 kHz): enable() no impone el valor pedido,
// impone el más cercano que la placa soporta y devuelve ESE. Si no se
// mira, el timeout real puede no ser el de REQUISITOS.md §3.11 y nadie
// se entera. Se informa por Serial: esta placa no tiene META.
void configurarWatchdog() {
  const int pedidoMs = COMETA_WATCHDOG_TIMEOUT_S * 1000;
  const int realMs = Watchdog.enable(pedidoMs);

  Serial.print("WATCHDOG: pedido ");
  Serial.print(pedidoMs);
  Serial.print(" ms, real ");
  Serial.print(realMs);
  Serial.println(" ms");

  if (realMs != pedidoMs) {
    Serial.println("WATCHDOG: el periodo real NO es el de REQUISITOS.md §3.11");
  }
}

// -----------------------------------------------------------------------
// Arranque
// -----------------------------------------------------------------------

// Monta la SD (chip select COMETA_SD_CS_PIN) y busca el primer número
// nnn libre para el archivo de esta corrida (REQUISITOS.md §2, §4.1).
void inicializarSD() {
  if (!SD.begin(COMETA_SD_CS_PIN)) {
    Serial.println("SD: no monta (chip select pin 4); se sigue sin log");
    return;
  }
  for (int n = 0; n < COMETA_L2_MAX_ARCHIVOS; n++) {
    snprintf(nombreL2, sizeof(nombreL2), COMETA_L2_PREFIJO "%03d" COMETA_L2_EXT, n);
    if (!SD.exists(nombreL2)) {
      return;
    }
  }
  Serial.println("SD: no queda ningún nnn libre (000-999)");
  nombreL2[0] = '\0';
}

// Crea L2_nnn.CSV y escribe su encabezado (REQUISITOS.md §4.1, §4.5). Un
// archivo nuevo en cada arranque: si la alimentación se corta, el
// anterior queda cerrado e intacto.
void abrirArchivoL2() {
  if (nombreL2[0] == '\0') {
    return;
  }
  archivoL2 = SD.open(nombreL2, FILE_WRITE);
  if (!archivoL2) {
    Serial.print("SD: no se pudo crear ");
    Serial.println(nombreL2);
    return;
  }
  escribirEncabezadoL2(archivoL2);
  archivoL2.flush();
  Serial.print("SD: escribiendo en ");
  Serial.println(nombreL2);
}

// -----------------------------------------------------------------------
// Verificación de configuración (valores [VERIFICAR] de
// config_adalogger.h todavía sin confirmar). Esta placa no tiene META:
// se informa por Serial.
// -----------------------------------------------------------------------

// Escribe "CONFIG INCOMPLETA: <nombre>" por Serial por cada centinela sin
// confirmar (los valores [VERIFICAR] no se inventan).
void reportarConfigIncompleta(const char *nombre) {
  Serial.print("CONFIG INCOMPLETA: ");
  Serial.println(nombre);
}

// Recorre las constantes marcadas [VERIFICAR] en config_adalogger.h y
// las informa por Serial antes de volar. Los demás centinelas ya se
// fijaron a valores decididos (REQUISITOS.md §2.1): solo queda el
// timeout I²C.
void verificarConfiguracion() {
  if (COMETA_I2C_TIMEOUT_MS == COMETA_MS_VERIFICAR) {
    reportarConfigIncompleta("I2C_TIMEOUT_MS");
  }
}

// -----------------------------------------------------------------------
// GPS — SAM-M8Q propio (REQUISITOS.md §3.1, §3.9, §4.5, §5)
// -----------------------------------------------------------------------

// Envía DYN_MODEL_AIRBORNE1g (CFG-NAV5 legacy), relee con
// getDynamicModel() y registra el resultado (REQUISITOS.md §3.1).
SFE_UBLOX_GNSS gps;
bool gpsPresente = false;
uint32_t proximoReintentoGPSMs = 0;

void recuperarBusI2C();

void inicializarGPS() {
  gpsPresente = gps.begin(Wire, COMETA_GPS_I2C_ADDR);
  if (!gpsPresente) {
    Serial.println("GPS: no responde en 0x42; se reintenta cada 30 s");
    proximoReintentoGPSMs = millis() + COMETA_GPS_REINTENTO_MS;
    return;
  }
  gps.setDynamicModel(DYN_MODEL_AIRBORNE1g);
  const bool airborne = (gps.getDynamicModel() == DYN_MODEL_AIRBORNE1g);
  Serial.println(airborne ? "GPS: modo Airborne <1g confirmado"
                          : "GPS: el modo Airborne <1g NO quedó (REQUISITOS.md §3.1)");
}

// Lee posición, altitud, vz_ms, sats, fix y UTC del GPS
// (REQUISITOS.md §3.9, §4.5). Vuelca en f, solo si hay un fix nuevo.
// utc se escribe SOLO con copiarUTC(f.utc, cadena) de log_format.h,
// nunca con strcpy() directo: es un char[21] justo y un desborde pisaría
// lat y lon (REQUISITOS.md §4.2).
void leerGPS(FilaL2 &f) {
  if (!gpsPresente) {
    if ((int32_t)(millis() - proximoReintentoGPSMs) >= 0) {
      inicializarGPS();
    }
    return;
  }
  if (!gps.getPVT(COMETA_GPS_ESPERA_PVT_MS)) {
    recuperarBusI2C();
    return;  // sin respuesta en este tic: celdas vacías
  }

  // El receptor contestó: fix y sats van siempre, dicen por qué no hay
  // posición. utc, posición, altitud y velocidad solo con fix 3D válido.
  const uint8_t tipoFix = gps.getFixType();
  f.fix = tipoFix;
  f.sats = gps.getSIV();

  if (tipoFix < 3 || !gps.getGnssFixOk()) {
    return;  // utc también vacío hasta tener fix (REQUISITOS.md §4.2)
  }
  if (gps.getTimeValid() && gps.getDateValid()) {
    char cadena[32];
    snprintf(cadena, sizeof(cadena), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             gps.getYear(), gps.getMonth(), gps.getDay(), gps.getHour(),
             gps.getMinute(), gps.getSecond());
    copiarUTC(f.utc, cadena);
  }
  f.lat = gps.getLatitude() / 1e7;          // 1e-7 grados -> grados
  f.lon = gps.getLongitude() / 1e7;
  f.alt_m = gps.getAltitudeMSL() / 1000.0f;  // mm -> m sobre el nivel del mar
  f.vz_ms = -gps.getNedDownVel() / 1000.0f;  // positiva hacia arriba (§4.2)
}

// -----------------------------------------------------------------------
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Si SDA queda baja: 9 pulsos en SCL y reinicializar (REQUISITOS.md
// §3.3). Se llama desde leerGPS() cuando detecta un error o timeout, no
// en cada vuelta de loop().
uint32_t recuperacionesI2C = 0;

void recuperarBusI2C() {
  Wire.end();
  pinMode(PIN_WIRE_SDA, INPUT_PULLUP);
  if (digitalRead(PIN_WIRE_SDA) == LOW) {
    // Algún dispositivo quedó a mitad de un byte sosteniendo SDA: con
    // pulsos de reloj termina de sacarlo y suelta la línea.
    pinMode(PIN_WIRE_SCL, OUTPUT);
    for (int i = 0; i < COMETA_I2C_RECOVERY_PULSOS_SCL; i++) {
      digitalWrite(PIN_WIRE_SCL, LOW);
      delayMicroseconds(5);
      digitalWrite(PIN_WIRE_SCL, HIGH);
      delayMicroseconds(5);
    }
    recuperacionesI2C++;
    Serial.print("I2C: SDA baja, recuperación #");
    Serial.println(recuperacionesI2C);
  }
  Wire.begin();
}

// -----------------------------------------------------------------------
// Batería (REQUISITOS.md §3.10, §4.5)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10). Vuelca en f, solo si hay un
// dato nuevo.
void leerBateria(FilaL2 &f) {
  const int lectura = analogRead(COMETA_VBATT_ADC_PIN);
  f.v_batt = lectura * (COMETA_ADC_VREF / ((1 << COMETA_ADC_BITS) - 1)) *
             COMETA_VBATT_DIVISOR_FACTOR;
}

// -----------------------------------------------------------------------
// Log L2 (REQUISITOS.md §2, §4.2, §4.5)
// -----------------------------------------------------------------------
//
// El encabezado (escribirEncabezadoL2), la struct FilaL2, limpiarFilaL2()
// y escribirFilaL2() están en include/log_format.h, compartido con
// src/teensy (REQUISITOS.md §1).

// flush() cada COMETA_FLUSH_CADA_N_MUESTRAS muestras (REQUISITOS.md §3.2).
void flushLogSiCorresponde() {
  static uint32_t filasSinFlush = 0;
  if (!archivoL2) {
    return;
  }
  if (++filasSinFlush >= COMETA_FLUSH_CADA_N_MUESTRAS) {
    archivoL2.flush();
    filasSinFlush = 0;
  }
}

// -----------------------------------------------------------------------
// Telemetría por radio, E220 en COMETA_E220_SERIAL (REQUISITOS.md §2.2)
// -----------------------------------------------------------------------

// Abre COMETA_E220_SERIAL a COMETA_E220_BAUD. M0 y M1 están a GND: el
// módulo ya está en modo transparente, no hay nada que configurar.
void inicializarRadio() {
  COMETA_E220_SERIAL.begin(COMETA_E220_BAUD);
}

// Print que escribe en un buffer en memoria: así la trama se arma entera
// con los mismos print(valor, decimales) del log y sale en una sola
// write() (en modo transparente el E220 corta el paquete cuando la UART
// queda en silencio, REQUISITOS.md §2.2).
class BufferTrama : public Print {
 public:
  size_t write(uint8_t c) override {
    if (largo >= sizeof(datos)) {
      return 0;  // no entra: se trunca, nunca se desborda
    }
    datos[largo++] = c;
    return 1;
  }
  uint8_t datos[COMETA_RADIO_LARGO_MAX + 2];  // + CR LF
  size_t largo = 0;
};

// Cada COMETA_RADIO_PERIODO_MS, también sin fix: arma la trama
// "id,n,hhmmss,lat,lon,alt_m,vz_ms,sats,fix,v_batt" en un buffer de
// COMETA_RADIO_LARGO_MAX con snprintf() (mismos decimales que L2, celda
// vacía si el campo es NAN o utc está vacío) y la manda con UNA sola
// println(), sin esperar respuesta y sin reintentar. n cuenta las tramas
// enviadas desde el arranque. Se llama DESPUÉS de escribir la fila en la
// SD: la radio nunca frena el log.
void enviarTramaRadio(const FilaL2 &f, uint32_t ahora) {
  static bool primera = true;
  static uint32_t ultimaTramaMs = 0;
  static uint32_t contador = 0;
  if (!primera && ahora - ultimaTramaMs < COMETA_RADIO_PERIODO_MS) {
    return;
  }
  primera = false;
  ultimaTramaMs = ahora;

  BufferTrama trama;
  trama.print(COMETA_RADIO_ID);
  trama.print(',');
  trama.print(contador++);
  trama.print(',');
  // utc compacto: "2026-10-15T13:02:05Z" -> "130205"; vacío sin hora.
  if (strlen(f.utc) >= 19) {
    const char hhmmss[7] = {f.utc[11], f.utc[12], f.utc[14],
                            f.utc[15], f.utc[17], f.utc[18], '\0'};
    trama.print(hhmmss);
  }
  // El resto, con las mismas macros de log_format.h que escriben la fila
  // L2: mismos decimales y misma celda vacía (un solo parser en tierra).
  // t_ms y utc no van: los reemplazan id, n y hhmmss.
#define RADIO_CAMPO_U32(campo, decimales)
#define RADIO_CAMPO_UTC(campo, decimales)
#define RADIO_CAMPO_LL(campo, decimales) \
  trama.print(',');                       \
  COMETA_CAMPO_ESCRIBE_LL(trama, campo, decimales);
#define RADIO_CAMPO_F(campo, decimales) \
  trama.print(',');                      \
  COMETA_CAMPO_ESCRIBE_F(trama, campo, decimales);
#define RADIO_CAMPO(nombre, tipo, decimales) RADIO_CAMPO_##tipo(f.nombre, decimales)
  COMETA_L2_CAMPOS(RADIO_CAMPO)
#undef RADIO_CAMPO
#undef RADIO_CAMPO_F
#undef RADIO_CAMPO_LL
#undef RADIO_CAMPO_UTC
#undef RADIO_CAMPO_U32
  trama.print("\r\n");

  COMETA_E220_SERIAL.write(trama.datos, trama.largo);  // una sola vez, sin esperar
  Serial.print("RADIO: ");
  Serial.write(trama.datos, trama.largo);
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

void setup() {
  // Serial primero, para que configurarWatchdog() pueda informar el
  // periodo real. Sin `while (!Serial)`: esperando al host USB la placa
  // no arrancaría nunca en vuelo.
  Serial.begin(115200);

  // El watchdog se arma lo primero de todo, antes de tocar nada: así
  // cubre también el arranque. A cambio hay que alimentarlo DESPUÉS DE
  // CADA PASO (REQUISITOS.md §3.11): ningún paso suelto puede pasarse de
  // COMETA_WATCHDOG_TIMEOUT_S, pero montar la SD, buscar el primer nnn
  // libre y configurar el GPS sí suman más sin que nada esté colgado, y
  // la placa se reiniciaría antes de llegar a loop(), una y otra vez.
  configurarWatchdog();

  analogReadResolution(COMETA_ADC_BITS);
  Wire.begin();

  verificarConfiguracion();
  Watchdog.reset();
  inicializarSD();
  Watchdog.reset();
  abrirArchivoL2();
  Watchdog.reset();
  inicializarGPS();
  Watchdog.reset();
  inicializarRadio();
  Watchdog.reset();
}

void loop() {
  Watchdog.reset();  // una sola vez por pasada de loop() (REQUISITOS.md §3.11)

  // Tick a 1 Hz (COMETA_TICK_L2_MS), sin delay() (REQUISITOS.md §4.1). Se
  // reprograma sumando el período, no fijándolo a "ahora", para no
  // acumular atraso; si se atrasó más de un período completo, se
  // realinea a "ahora" en vez de intentar recuperarlo.
  const unsigned long ahora = millis();
  static unsigned long ultimoTickL2 = 0;

  if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
    ultimoTickL2 += COMETA_TICK_L2_MS;
    if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
      ultimoTickL2 = ahora;
    }

    limpiarFilaL2(filaL2);
    filaL2.t_ms = ahora;

    leerGPS(filaL2);
    leerBateria(filaL2);

    escribirFilaL2(archivoL2, filaL2);
    flushLogSiCorresponde();

    // Después de la SD, nunca antes (REQUISITOS.md §2.2).
    enviarTramaRadio(filaL2, ahora);
  }
}
