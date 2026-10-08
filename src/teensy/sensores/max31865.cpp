// max31865.cpp
//
// 2x MAX31865 + PT1000, brazo exterior y tubo (REQUISITOS.md §4.3, §5).
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): begin(MAX31865_3WIRE), RNOMINAL = 1000 y RREF = 4300 (los
// valores correctos para PT1000), y la idea de leer el fault byte para
// diagnosticar la sonda. Cambios respecto del banco de prueba, para
// cumplir REQUISITOS.md:
//   - Dos instancias (brazo, CS 10; tubo, CS 9) con las constantes de
//     config_teensy.h, no números sueltos.
//   - Presencia real: Adafruit_MAX31865::begin() devuelve true siempre,
//     aunque no haya nada conectado. Acá se relee el registro de
//     configuración: si no vuelve lo que se escribió, no hay chip.
//   - Sin bloqueo (§1.2): temperature() de la librería hace un disparo
//     único y espera ~75 ms con delay(), dos veces por tic. Acá el chip
//     queda en conversión continua (autoConvert) y se lee el último
//     resultado del registro RTD, sin esperar.
//   - El fault byte va al log (err_arm / err_tubo) y se limpia.
//   - La librería se usa solo para begin() y la configuración; las
//     lecturas van por registro, con SPI directo.

#include "max31865.h"

#include <Adafruit_MAX31865.h>
#include <SPI.h>
#include <math.h>

#include "config_teensy.h"

namespace {

// Registros del MAX31865 (hoja de datos, tabla 1).
const uint8_t REG_CONFIG = 0x00;
const uint8_t REG_RTD_MSB = 0x01;
const uint8_t REG_FALLAS = 0x07;
const uint8_t BIT_BORRAR_FALLAS = 0x02;

// Configuración esperada: bias encendido (0x80), conversión continua
// (0x40), 3 hilos (0x10) y filtro de 50 Hz (0x01, la red en Uruguay).
// El bit 0x02 (borrar fallas) se apaga solo, por eso se enmascara.
const uint8_t CONFIG_ESPERADA = 0x80 | 0x40 | 0x10 | 0x01;
const uint8_t CONFIG_MASCARA = 0xFD;

// Mismo modo SPI y velocidad que usa la librería de Adafruit.
const SPISettings AJUSTES_SPI(1000000, MSBFIRST, SPI_MODE1);

// Coeficientes de Callendar–Van Dusen (IEC 60751), los mismos que usa
// Adafruit_MAX31865::temperature().
const float RTD_A = 3.9083e-3f;
const float RTD_B = -5.775e-7f;

// Una sonda: el chip, su CS y la última lectura.
struct SondaPT1000 {
  Adafruit_MAX31865 chip;
  uint8_t cs;
  bool datoNuevo;
  float temperatura;
  float falla;

  explicit SondaPT1000(uint8_t pinCS)
      : chip(pinCS), cs(pinCS), datoNuevo(false), temperatura(NAN), falla(NAN) {}
};

SondaPT1000 brazo(COMETA_MAX31865_CS_ARM_PIN);
SondaPT1000 tubo(COMETA_MAX31865_CS_TUBO_PIN);

// Lee n bytes desde el registro reg (lectura: bit 7 de la dirección en 0).
void leerRegistros(uint8_t cs, uint8_t reg, uint8_t *datos, uint8_t n) {
  SPI.beginTransaction(AJUSTES_SPI);
  digitalWrite(cs, LOW);
  SPI.transfer(reg & 0x7F);
  for (uint8_t i = 0; i < n; i++) {
    datos[i] = SPI.transfer(0xFF);
  }
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

// Escribe un registro (escritura: bit 7 de la dirección en 1).
void escribirRegistro(uint8_t cs, uint8_t reg, uint8_t valor) {
  SPI.beginTransaction(AJUSTES_SPI);
  digitalWrite(cs, LOW);
  SPI.transfer(reg | 0x80);
  SPI.transfer(valor);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

bool configuracionCorrecta(const SondaPT1000 &s) {
  uint8_t config = 0;
  leerRegistros(s.cs, REG_CONFIG, &config, 1);
  return (config & CONFIG_MASCARA) == CONFIG_ESPERADA;
}

// Resistencia -> temperatura. Por encima de 0 °C, la inversa exacta de
// Callendar–Van Dusen; por debajo, el polinomio de ajuste de Adafruit
// (para una PT100 equivalente), que en la estratosfera es el que manda.
float temperaturaDesdeResistencia(float r) {
  const float z1 = -RTD_A;
  const float z2 = RTD_A * RTD_A - 4 * RTD_B;
  const float z3 = (4 * RTD_B) / COMETA_MAX31865_RNOMINAL;
  const float z4 = 2 * RTD_B;
  float t = (sqrtf(z2 + z3 * r) + z1) / z4;
  if (t >= 0) {
    return t;
  }
  const float r100 = r / COMETA_MAX31865_RNOMINAL * 100.0f;
  float potencia = r100;
  t = -242.02f;
  t += 2.2228f * potencia;
  potencia *= r100;
  t += 2.5859e-3f * potencia;
  potencia *= r100;
  t -= 4.8260e-6f * potencia;
  potencia *= r100;
  t -= 2.8183e-8f * potencia;
  potencia *= r100;
  t += 1.5243e-10f * potencia;
  return t;
}

bool iniciarSonda(SondaPT1000 &s) {
  s.chip.begin(COMETA_MAX31865_WIRING);
  s.chip.enable50Hz(true);
  s.chip.enableBias(true);
  s.chip.autoConvert(true);
  return configuracionCorrecta(s);
}

bool actualizarSonda(SondaPT1000 &s) {
  s.datoNuevo = false;
  // Si el chip se reinició o se desconectó, la configuración ya no es la
  // nuestra: lectura fallida (REQUISITOS.md §1.3).
  if (!configuracionCorrecta(s)) {
    return false;
  }

  uint8_t rtd[2];
  leerRegistros(s.cs, REG_RTD_MSB, rtd, 2);
  const uint16_t crudo = ((uint16_t)rtd[0] << 8 | rtd[1]) >> 1;  // bit 0 = falla

  // El fault byte va siempre al log (0 = sin falla) y la temperatura
  // también: los datos dudosos se marcan, no se descartan (§3.7). Se lee
  // el registro directo y no con readFault(): con su argumento por
  // defecto, la librería lanza un ciclo de diagnóstico que reescribe la
  // configuración y apaga la conversión continua.
  uint8_t falla = 0;
  leerRegistros(s.cs, REG_FALLAS, &falla, 1);
  if (falla != 0) {
    escribirRegistro(s.cs, REG_CONFIG, CONFIG_ESPERADA | BIT_BORRAR_FALLAS);
  }
  s.falla = falla;
  s.temperatura = temperaturaDesdeResistencia(crudo / 32768.0f * COMETA_MAX31865_RREF);
  s.datoNuevo = true;
  return true;
}

}  // namespace

namespace SensorPT1000Brazo {

bool iniciar() { return iniciarSonda(brazo); }

bool actualizar(uint32_t ahora) {
  (void)ahora;
  return actualizarSonda(brazo);
}

void llenarFila(FilaSCI &f) {
  if (!brazo.datoNuevo) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.t_arm_C = brazo.temperatura;
  f.err_arm = brazo.falla;
}

}  // namespace SensorPT1000Brazo

namespace SensorPT1000Tubo {

bool iniciar() { return iniciarSonda(tubo); }

bool actualizar(uint32_t ahora) {
  (void)ahora;
  return actualizarSonda(tubo);
}

void llenarFila(FilaSCI &f) {
  if (!tubo.datoNuevo) {
    return;  // celda vacía: no hubo lectura nueva en esta fila (REQUISITOS.md §4.2)
  }
  f.t_tubo_C = tubo.temperatura;
  f.err_tubo = tubo.falla;
}

}  // namespace SensorPT1000Tubo
