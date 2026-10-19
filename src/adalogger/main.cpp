// COMETA · Feather M0 Adalogger (nivel 2) — posición y altitud de respaldo
//
// GPS SAM-M8Q por I²C -> fila L2 en la SD (L2_nnn.CSV) cada 1 s ->
// trama de radio por el E220 cada COMETA_RADIO_PERIODO_MS
// (REQUISITOS.md §2, §2.2, §4.5). Mensajes de diagnóstico por Serial:
// esta placa no tiene META. La fila L2 sale también por Serial, con el
// prefijo "L2: ", para probar la placa en el banco con el monitor serie.
//
// Esta placa es independiente de la del Teensy (REQUISITOS.md §2): no
// asume nada de la otra cadena. La única relación entre los dos logs es
// el UTC del GPS.
//
// Ninguna falla detiene a las demás: sin SD sigue la radio, sin GPS
// siguen la SD (t_ms y v_batt) y la radio (la trama dice que la sonda
// está viva), y la SD y el GPS se reintentan solos.

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
char nombreL2[16] = "";  // "L2_nnn.CSV" del archivo abierto, o el último
// Próximo intento de montar la SD si no está grabando (al arrancar sin
// tarjeta, o después de un error de escritura en vuelo).
uint32_t proximoReintentoSDMs = 0;

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

// Causa del último reinicio (REQUISITOS.md §3.11): el registro RCAUSE del
// SAMD21, que expone resetCause(). Un reinicio en vuelo se ve en la SD
// como un L2_nnn.CSV nuevo y en tierra como n que vuelve a 0; esto dice
// por qué. Va por Serial: esta placa no tiene META; se repite en la línea
// ESTADO (reportarEstado()).
const char *causaReinicio = "desconocida";

void reportarCausaReinicio() {
  const uint8_t causa = Watchdog.resetCause();
  const char *nombre = "desconocida";
  if (causa & PM_RCAUSE_WDT) {
    nombre = "watchdog";
  } else if (causa & PM_RCAUSE_SYST) {
    nombre = "reinicio por software";
  } else if (causa & PM_RCAUSE_EXT) {
    nombre = "botón de reset";
  } else if (causa & (PM_RCAUSE_BOD12 | PM_RCAUSE_BOD33)) {
    nombre = "caída de tensión (brown-out)";
  } else if (causa & PM_RCAUSE_POR) {
    nombre = "encendido";
  }
  causaReinicio = nombre;
  Serial.print("ARRANQUE: causa del reinicio = ");
  Serial.print(nombre);
  Serial.print(" (RCAUSE 0x");
  Serial.print(causa, HEX);
  Serial.println(")");
}

// -----------------------------------------------------------------------
// SD (REQUISITOS.md §2, §4.1)
// -----------------------------------------------------------------------

// Monta la SD (chip select COMETA_SD_CS_PIN), busca el primer nnn libre
// y crea L2_nnn.CSV con su encabezado (REQUISITOS.md §4.1, §4.5). Un
// archivo nuevo en cada arranque y en cada vuelta de la SD después de un
// error: el anterior queda como estaba. true si el archivo quedó abierto
// y el encabezado escrito.
bool abrirArchivoL2() {
  if (!SD.begin(COMETA_SD_CS_PIN)) {
    Serial.println("SD: no monta (chip select pin 4); se sigue sin log");
    return false;
  }
  int n = 0;
  for (; n < COMETA_L2_MAX_ARCHIVOS; n++) {
    snprintf(nombreL2, sizeof(nombreL2), COMETA_L2_PREFIJO "%03d" COMETA_L2_EXT, n);
    if (!SD.exists(nombreL2)) {
      break;
    }
  }
  if (n == COMETA_L2_MAX_ARCHIVOS) {
    Serial.println("SD: no queda ningún nnn libre (000-999)");
    return false;
  }
  archivoL2 = SD.open(nombreL2, FILE_WRITE);
  if (!archivoL2) {
    Serial.print("SD: no se pudo crear ");
    Serial.println(nombreL2);
    return false;
  }
  escribirEncabezadoL2(archivoL2);
  archivoL2.flush();
  if (archivoL2.getWriteError()) {
    Serial.print("SD: no se pudo escribir el encabezado de ");
    Serial.println(nombreL2);
    archivoL2.close();
    return false;
  }
  Serial.print("SD: escribiendo en ");
  Serial.println(nombreL2);
  return true;
}

// Si la SD no está grabando, la vuelve a montar cada
// COMETA_SD_REINTENTO_MS. Se llama al final del tic, después de la radio:
// un SD.begin() sin tarjeta tarda hasta ~2 s y no tiene que atrasar la
// trama de este tic.
void reintentarSDSiCorresponde(uint32_t ahora) {
  if (archivoL2 || (int32_t)(ahora - proximoReintentoSDMs) < 0) {
    return;
  }
  if (!abrirArchivoL2()) {
    proximoReintentoSDMs = ahora + COMETA_SD_REINTENTO_MS;
  }
}

// Escribe la fila en L2_nnn.CSV, con flush() cada
// COMETA_FLUSH_CADA_N_MUESTRAS filas (REQUISITOS.md §3.2). true si la
// fila quedó escrita. Si la escritura falla (tarjeta que se movió, un
// corte breve de alimentación de la SD), cierra el archivo y programa el
// reintento: la SD no vuelve sola, y seguir escribiendo en un archivo
// roto perdería todo el resto del vuelo sin que nadie se entere.
bool escribirFilaSD(const FilaL2 &f, uint32_t ahora) {
  static uint32_t filasSinFlush = 0;
  if (!archivoL2) {
    return false;
  }
  escribirFilaL2(archivoL2, f);
  if (++filasSinFlush >= COMETA_FLUSH_CADA_N_MUESTRAS) {
    archivoL2.flush();
    filasSinFlush = 0;
  }
  // getWriteError() queda en true desde la primera write() que falló.
  // La librería guarda las filas en un bloque de 512 bytes y lo escribe
  // en la tarjeta cuando se llena: una SD muerta se nota a las pocas
  // filas, no necesariamente en la misma.
  if (archivoL2.getWriteError()) {
    Serial.print("SD: error de escritura en ");
    Serial.print(nombreL2);
    Serial.println("; se cierra y se reintenta en 30 s con un archivo nuevo");
    archivoL2.close();
    filasSinFlush = 0;
    proximoReintentoSDMs = ahora + COMETA_SD_REINTENTO_MS;
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------
// LEDs de estado (config_adalogger.h): un destello por tic, verde si la
// fila se escribió en la SD, rojo si no. Sin delay(): se encienden en el
// tic y loop() los apaga pasado COMETA_LED_DESTELLO_MS.
// -----------------------------------------------------------------------
uint32_t ledEncendidoMs = 0;
bool ledEncendido = false;

void iniciarLeds() {
  pinMode(COMETA_LED_SD_OK_PIN, OUTPUT);
  pinMode(COMETA_LED_SD_ERROR_PIN, OUTPUT);
  digitalWrite(COMETA_LED_SD_OK_PIN, LOW);
  digitalWrite(COMETA_LED_SD_ERROR_PIN, LOW);
}

void destellarLed(bool sdOk, uint32_t ahora) {
  digitalWrite(sdOk ? COMETA_LED_SD_OK_PIN : COMETA_LED_SD_ERROR_PIN, HIGH);
  ledEncendidoMs = ahora;
  ledEncendido = true;
}

void apagarLedsSiCorresponde(uint32_t ahora) {
  if (ledEncendido && ahora - ledEncendidoMs >= COMETA_LED_DESTELLO_MS) {
    digitalWrite(COMETA_LED_SD_OK_PIN, LOW);
    digitalWrite(COMETA_LED_SD_ERROR_PIN, LOW);
    ledEncendido = false;
  }
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
// Recuperación del bus I²C (REQUISITOS.md §3.3)
// -----------------------------------------------------------------------

// Si SDA quedó baja: 9 pulsos en SCL y reinicializar Wire (REQUISITOS.md
// §3.3). Se llama antes de cada intento de inicializarGPS(), al arrancar
// incluido: un GPS que quedó a mitad de un byte cuando se reinició el
// Feather (el watchdog, el botón de reset) sostiene SDA baja y begin()
// fallaría siempre, en cada reintento, sin que nada liberara el bus.
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
// GPS — SAM-M8Q propio (REQUISITOS.md §3.1, §3.9, §4.5, §5)
//
// Igual que en el Teensy (src/teensy/sensores/gps.cpp, probado en el
// banco):
//   - setI2COutput(COM_TYPE_UBX): por I²C solo UBX, sin NMEA que leer y
//     descartar en cada tic.
//   - setAutoPVT(true): el receptor manda NAV-PVT solo, una vez por
//     solución, y getPVT() lee lo que haya llegado sin esperar. Pedirlo
//     con un poll obligaba a esperar la respuesta en cada tic.
//   - Configuración en cada arranque, sin saveConfiguration(): no hace
//     falta guardarla, se vuelve a mandar cada vez.
// Propio de esta placa: si pasa COMETA_GPS_SIN_PVT_MAX_MS sin NAV-PVT,
// se recupera el bus y se reinicializa enseguida. Un SAM-M8Q que se
// reinició (una caída de tensión del pin 3V) vuelve con la configuración
// de fábrica: modo Portable, que deja de dar fix por encima de 12 km, y
// sin NAV-PVT automático. Es el único sensor de esta placa: no se espera
// al reintento de 30 s para devolverle el modo Airborne. Solo si ya
// había llegado algún NAV-PVT desde la última inicialización: si no,
// reinicializarlo de nuevo no cambia nada y se espera el reintento.
// -----------------------------------------------------------------------
SFE_UBLOX_GNSS gps;
bool gpsPresente = false;
// Resultado de la relectura del modelo en la última inicialización
// (REQUISITOS.md §3.1), para la línea ESTADO.
bool gpsAirborne = false;
uint32_t proximoReintentoGPSMs = 0;
uint32_t ultimoPVTMs = 0;
bool huboPVTDesdeInicio = false;

// begin() en 0x42, solo UBX por I²C, DYN_MODEL_AIRBORNE1g (CFG-NAV5
// legacy) releído con getDynamicModel() (REQUISITOS.md §3.1) y NAV-PVT
// automático. Si algo falla, se reintenta en COMETA_GPS_REINTENTO_MS.
void inicializarGPS() {
  const uint32_t ahora = millis();
  recuperarBusI2C();
  gpsPresente = false;

  if (!gps.begin(Wire, COMETA_GPS_I2C_ADDR)) {
    Serial.println("GPS: no responde en 0x42; se reintenta cada 30 s");
    proximoReintentoGPSMs = ahora + COMETA_GPS_REINTENTO_MS;
    return;
  }
  gps.setI2COutput(COM_TYPE_UBX);
  gps.setDynamicModel(DYN_MODEL_AIRBORNE1g);
  const bool airborne = (gps.getDynamicModel() == DYN_MODEL_AIRBORNE1g);
  gpsAirborne = airborne;
  Serial.println(airborne ? "GPS: modo Airborne <1g confirmado"
                          : "GPS: el modo Airborne <1g NO quedó (REQUISITOS.md §3.1)");
  if (!gps.setAutoPVT(true)) {
    // Sin NAV-PVT automático getPVT() no traería nada.
    Serial.println("GPS: no aceptó NAV-PVT automático; se reintenta en 30 s");
    proximoReintentoGPSMs = ahora + COMETA_GPS_REINTENTO_MS;
    return;
  }
  gpsPresente = true;
  huboPVTDesdeInicio = false;
  ultimoPVTMs = millis();
}

// Lee posición, altitud, vz_ms, sats, fix y UTC del GPS
// (REQUISITOS.md §3.9, §4.5). Vuelca en f, solo si llegó un NAV-PVT
// nuevo. utc se escribe SOLO con copiarUTC(f.utc, cadena) de
// log_format.h, nunca con strcpy() directo: es un char[21] justo y un
// desborde pisaría lat y lon (REQUISITOS.md §4.2).
void leerGPS(FilaL2 &f, uint32_t ahora) {
  if (!gpsPresente) {
    if ((int32_t)(ahora - proximoReintentoGPSMs) >= 0) {
      inicializarGPS();
    }
    return;
  }
  if (!gps.getPVT()) {
    // Todavía no llegó un NAV-PVT nuevo: celdas vacías en esta fila.
    if (ahora - ultimoPVTMs < COMETA_GPS_SIN_PVT_MAX_MS) {
      return;
    }
    if (huboPVTDesdeInicio) {
      Serial.println("GPS: sin NAV-PVT; se recupera el bus y se reinicializa");
      inicializarGPS();
    } else {
      Serial.println("GPS: ningún NAV-PVT desde que se inicializó; se reintenta en 30 s");
      gpsPresente = false;
      proximoReintentoGPSMs = ahora + COMETA_GPS_REINTENTO_MS;
    }
    return;
  }
  ultimoPVTMs = ahora;
  huboPVTDesdeInicio = true;

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
// Batería (REQUISITOS.md §3.10, §4.5)
// -----------------------------------------------------------------------

// Lee v_batt por ADC (REQUISITOS.md §3.10): promedio de
// COMETA_VBATT_MUESTRAS lecturas, unos 4 ms en total.
void leerBateria(FilaL2 &f) {
  uint32_t suma = 0;
  for (int i = 0; i < COMETA_VBATT_MUESTRAS; i++) {
    suma += analogRead(COMETA_VBATT_ADC_PIN);
  }
  const float lectura = (float)suma / COMETA_VBATT_MUESTRAS;
  f.v_batt = lectura * (COMETA_ADC_VREF / ((1 << COMETA_ADC_BITS) - 1)) *
             COMETA_VBATT_DIVISOR_FACTOR;
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
// COMETA_RADIO_LARGO_MAX (mismos decimales que L2, celda vacía si el
// campo es NAN o utc está vacío) y la manda con UNA sola write(), sin
// esperar respuesta y sin reintentar. n cuenta las tramas enviadas desde
// el arranque. Se llama DESPUÉS de escribir la fila en la SD: la radio
// nunca frena el log.
//
// El periodo se cuenta en tics (una trama cada 5), no comparando millis()
// con el instante de la trama anterior: los tics llegan con unos ms de
// variación, y a 4999 ms la trama se pasaba al tic siguiente. En el banco
// salían cada 5 o 6 s, alternando.
static_assert(COMETA_RADIO_PERIODO_MS % COMETA_TICK_L2_MS == 0,
              "COMETA_RADIO_PERIODO_MS tiene que ser un múltiplo del tic");
const uint32_t TICS_POR_TRAMA = COMETA_RADIO_PERIODO_MS / COMETA_TICK_L2_MS;

void enviarTramaRadio(const FilaL2 &f) {
  // La primera trama sale en el primer tic: n = 0 avisa del arranque.
  static uint32_t ticsDesdeTrama = TICS_POR_TRAMA - 1;
  static uint32_t contador = 0;
  if (++ticsDesdeTrama < TICS_POR_TRAMA) {
    return;
  }
  ticsDesdeTrama = 0;

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
// Línea de estado por Serial (COMETA_ESTADO_PERIODO_MS)
// -----------------------------------------------------------------------

// Repite cada COMETA_ESTADO_PERIODO_MS lo que setup() informa una sola
// vez: con el USB nativo del Feather M0, esas líneas salen antes de que
// el monitor serie se conecte y se pierden. Ejemplo:
//   ESTADO: gps=si airborne=si sd=L2_003.CSV i2c_recov=0 reinicio=encendido
void reportarEstado(uint32_t ahora) {
  static bool primera = true;
  static uint32_t ultimoEstadoMs = 0;
  if (!primera && ahora - ultimoEstadoMs < COMETA_ESTADO_PERIODO_MS) {
    return;
  }
  primera = false;
  ultimoEstadoMs = ahora;

  Serial.print("ESTADO: gps=");
  Serial.print(gpsPresente ? "si" : "no");
  Serial.print(" airborne=");
  Serial.print(gpsPresente ? (gpsAirborne ? "si" : "NO") : "-");
  Serial.print(" sd=");
  Serial.print(archivoL2 ? nombreL2 : "no");
  Serial.print(" i2c_recov=");
  Serial.print(recuperacionesI2C);
  Serial.print(" reinicio=");
  Serial.println(causaReinicio);
}

// -----------------------------------------------------------------------
// setup() / loop()
// -----------------------------------------------------------------------

void setup() {
  // Serial primero, para que configurarWatchdog() pueda informar el
  // periodo real. Sin `while (!Serial)`: esperando al host USB la placa
  // no arrancaría nunca en vuelo. Sin host, print() no hace nada.
  Serial.begin(115200);

  // El watchdog se arma lo primero de todo, antes de tocar nada: así
  // cubre también el arranque. A cambio hay que alimentarlo DESPUÉS DE
  // CADA PASO (REQUISITOS.md §3.11): ningún paso suelto puede pasarse de
  // COMETA_WATCHDOG_TIMEOUT_S, pero montar la SD, buscar el primer nnn
  // libre y configurar el GPS sí suman más sin que nada esté colgado, y
  // la placa se reiniciaría antes de llegar a loop(), una y otra vez.
  configurarWatchdog();
  reportarCausaReinicio();

  iniciarLeds();
  analogReadResolution(COMETA_ADC_BITS);
  Wire.begin();

  verificarConfiguracion();
  Watchdog.reset();
  if (!abrirArchivoL2()) {
    proximoReintentoSDMs = millis() + COMETA_SD_REINTENTO_MS;
  }
  Watchdog.reset();
  inicializarGPS();
  Watchdog.reset();
  inicializarRadio();
  Watchdog.reset();
}

void loop() {
  Watchdog.reset();  // una sola vez por pasada de loop() (REQUISITOS.md §3.11)

  const uint32_t ahora = millis();
  apagarLedsSiCorresponde(ahora);

  // Tick a 1 Hz (COMETA_TICK_L2_MS), sin delay() (REQUISITOS.md §1.1). Se
  // reprograma sumando el período, no fijándolo a "ahora", para no
  // acumular atraso; si se atrasó más de un período completo, se
  // realinea a "ahora" en vez de intentar recuperarlo.
  static uint32_t ultimoTickL2 = 0;

  if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
    ultimoTickL2 += COMETA_TICK_L2_MS;
    if (ahora - ultimoTickL2 >= COMETA_TICK_L2_MS) {
      ultimoTickL2 = ahora;
    }

    limpiarFilaL2(filaL2);
    filaL2.t_ms = ahora;

    leerGPS(filaL2, ahora);
    leerBateria(filaL2);

    const bool sdOk = escribirFilaSD(filaL2, ahora);
    destellarLed(sdOk, ahora);
    Serial.print("L2: ");
    escribirFilaL2(Serial, filaL2);

    // Después de la SD, nunca antes (REQUISITOS.md §2.2).
    enviarTramaRadio(filaL2);

    // Al final: un SD.begin() sin tarjeta tarda y no atrasa la trama.
    reintentarSDSiCorresponde(ahora);
    reportarEstado(ahora);
  }
}
