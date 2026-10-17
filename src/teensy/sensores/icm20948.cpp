// icm20948.cpp
//
// ICM-20948 (IMU, I²C 0x69). REQUISITOS.md §4.3, §4.6, §5, §7.
//
// Base: el banco de prueba de los estudiantes (COMETA-Teensy41,
// main.cpp): begin(Wire, 1) (AD0 alto = 0x69, correcto), dataReady() y
// getAGMT(). Cambios respecto del banco de prueba, para cumplir
// REQUISITOS.md:
//   - Unidades físicas: icm.agmt.acc.axes.x y compañía son conteos
//     crudos del conversor, no mg ni °/s. Acá se usan accX() (mg, que se
//     pasa a g), gyrX() (°/s) y magX() (µT), que ya aplican la escala.
//   - Fondo de escala ±16 g (§5): con el de fábrica (±2 g) el estallido
//     satura. Giróscopo a ±2000 °/s por la misma razón.
//   - Muestreo a ~100 Hz con filtro antialiasing (§7).
//   - Se agregan mx, my, mz (§4.3).
//
//   - Lectura desde la FIFO del chip, no del registro de la última
//     muestra (§1.2, §7): acelerómetro y giróscopo entran en la FIFO a
//     100 Hz y cada pasada del tic de 10 ms la vacía entera. Así un tic
//     atrasado (el de 1000 ms bloquea hasta ~20 ms por módulo) entrega
//     varias muestras juntas en vez de perderlas. Cada muestra lleva su
//     t_ms, contado hacia atrás desde la lectura a 10 ms por muestra.
//   - El magnetómetro no entra en la FIFO (va por el maestro I²C interno
//     del chip): se relee con getAGMT() cada
//     COMETA_ICM20948_MAG_CADA_N_TICS tics y cada muestra lleva el último
//     valor.
//   - Las muestras se guardan crudas (MuestraIMU, enteros del chip): el
//     buffer circular de la ventana es binario (§7) y la conversión a
//     unidades se hace recién al escribir IMU_nnn.CSV.

#include "icm20948.h"

#include <ICM_20948.h>
#include <Wire.h>

#include "config_teensy.h"

namespace SensorICM20948 {

namespace {

ICM_20948_I2C icm;

// AD0 alto: dirección 0x69 (COMETA_ICM20948_I2C_ADDR). La librería
// recibe el valor del pin, no la dirección.
const uint8_t VALOR_AD0 = (COMETA_ICM20948_I2C_ADDR == 0x69) ? 1 : 0;

// Escalas del chip con los fondos de escala de iniciar(): ±16 g y
// ±2000 °/s en 16 bits con signo; magnetómetro 0,15 µT por cuenta
// (AK09916, el mismo factor que usa magX() de la librería).
const float LSB_POR_G = 32768.0f / COMETA_ICM20948_RANGO_G;
const float LSB_POR_DPS = 32768.0f / COMETA_ICM20948_RANGO_DPS;
const float UT_POR_LSB_MAG = 0.15f;

// FIFO: registros del banco 0 (hoja de datos, tabla de registros).
// FIFO_EN_2: ACCEL_FIFO_EN (0x10) + GYRO_Z/Y/X_FIFO_EN (0x08|0x04|0x02).
const uint8_t FIFO_EN_2_ACC_GYR = 0x10 | 0x08 | 0x04 | 0x02;
// Orden de cada muestra en la FIFO: ax ay az gx gy gz, 16 bits big endian.
const uint8_t BYTES_POR_MUESTRA = 12;
const size_t MAX_MUESTRAS_POR_LECTURA = COMETA_ICM20948_FIFO_MAX_BYTES / BYTES_POR_MUESTRA;

MuestraIMU nuevas[MAX_MUESTRAS_POR_LECTURA];
size_t cantidadNuevas = 0;

// Última muestra para la fila SCI; hayUltima = llegó alguna desde la
// fila anterior (§4.2).
MuestraIMU ultima = {};
bool hayUltima = false;

int16_t magActual[3] = {};
uint8_t ticsDesdeMag = 0;
uint32_t totalMuestras = 0;
uint32_t totalResincronizaciones = 0;

int16_t palabraBE(const uint8_t *b) { return (int16_t)((b[0] << 8) | b[1]); }

bool configurarFIFO() {
  icm.setBank(0);
  uint8_t ninguno = 0x00;  // FIFO_EN_1: ningún esclavo del maestro I²C
  icm.write(AGB0_REG_FIFO_EN_1, &ninguno, 1);
  uint8_t accGyr = FIFO_EN_2_ACC_GYR;
  icm.write(AGB0_REG_FIFO_EN_2, &accGyr, 1);
  icm.setFIFOmode(false);  // stream: si se llena, pisa lo más viejo
  icm.resetFIFO();
  icm.enableFIFO(true);
  return icm.status == ICM_20948_Stat_Ok;
}

}  // namespace

bool iniciar() {
  icm.begin(Wire, VALOR_AD0);
  if (icm.status != ICM_20948_Stat_Ok) {
    return false;
  }

  const uint8_t accGyr = ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr;

  ICM_20948_fss_t escala;
  escala.a = gpm16;    // ±16 g (COMETA_ICM20948_RANGO_G)
  escala.g = dps2000;  // ±2000 °/s
  icm.setFullScale(accGyr, escala);

  // Filtro pasabajos a ~24 Hz, por debajo de Nyquist para 100 Hz.
  ICM_20948_dlpcfg_t filtro;
  filtro.a = acc_d23bw9_n34bw4;
  filtro.g = gyr_d23bw9_n35bw9;
  icm.setDLPFcfg(accGyr, filtro);
  icm.enableDLPF(ICM_20948_Internal_Acc, true);
  icm.enableDLPF(ICM_20948_Internal_Gyr, true);

  // Divisores de la hoja de datos: acelerómetro 1125/(1+10) ≈ 102 Hz,
  // giróscopo 1100/(1+10) = 100 Hz (COMETA_IMU_TASA_HZ).
  ICM_20948_smplrt_t tasa;
  tasa.a = 10;
  tasa.g = 10;
  icm.setSampleRate(accGyr, tasa);
  if (icm.status != ICM_20948_Stat_Ok) {
    return false;
  }

  cantidadNuevas = 0;
  hayUltima = false;
  ticsDesdeMag = COMETA_ICM20948_MAG_CADA_N_TICS;  // leer el mag en la primera pasada
  return configurarFIFO();
}

bool actualizar(uint32_t ahora) {
  cantidadNuevas = 0;

  if (++ticsDesdeMag >= COMETA_ICM20948_MAG_CADA_N_TICS) {
    ticsDesdeMag = 0;
    icm.getAGMT();
    if (icm.status != ICM_20948_Stat_Ok) {
      return false;
    }
    magActual[0] = icm.agmt.mag.axes.x;
    magActual[1] = icm.agmt.mag.axes.y;
    magActual[2] = icm.agmt.mag.axes.z;
  }

  uint16_t bytes = 0;
  if (icm.getFIFOcount(&bytes) != ICM_20948_Stat_Ok) {
    return false;
  }
  // Desalineada (un resto que no cierra en 12) o casi llena: lo que hay
  // ya no se puede partir en muestras confiables. Se descarta y se vuelve
  // a empezar; el contador va a META.
  if (bytes % BYTES_POR_MUESTRA != 0 || bytes > COMETA_ICM20948_FIFO_MAX_BYTES) {
    totalResincronizaciones++;
    return icm.resetFIFO() == ICM_20948_Stat_Ok;
  }

  const size_t enFIFO = bytes / BYTES_POR_MUESTRA;
  for (size_t i = 0; i < enFIFO; i++) {
    uint8_t b[BYTES_POR_MUESTRA];
    if (icm.readFIFO(b, BYTES_POR_MUESTRA) != ICM_20948_Stat_Ok) {
      return false;
    }
    MuestraIMU &m = nuevas[cantidadNuevas++];
    // La más nueva es la de ahora; las anteriores, 10 ms antes cada una.
    m.t_ms = ahora - (uint32_t)(enFIFO - 1 - i) * COMETA_TICK_IMU_MS;
    for (uint8_t eje = 0; eje < 3; eje++) {
      m.a[eje] = palabraBE(&b[2 * eje]);
      m.g[eje] = palabraBE(&b[6 + 2 * eje]);
      m.m[eje] = magActual[eje];
    }
  }
  if (cantidadNuevas > 0) {
    ultima = nuevas[cantidadNuevas - 1];
    hayUltima = true;
    totalMuestras += cantidadNuevas;
  }
  return true;
}

size_t tomarMuestras(const MuestraIMU **muestras) {
  *muestras = nuevas;
  const size_t n = cantidadNuevas;
  cantidadNuevas = 0;  // cada muestra se entrega una sola vez
  return n;
}

void aFilaIMU(const MuestraIMU &m, FilaIMU &f) {
  f.t_ms = m.t_ms;
  f.ax = m.a[0] / LSB_POR_G;
  f.ay = m.a[1] / LSB_POR_G;
  f.az = m.a[2] / LSB_POR_G;
  f.gx = m.g[0] / LSB_POR_DPS;
  f.gy = m.g[1] / LSB_POR_DPS;
  f.gz = m.g[2] / LSB_POR_DPS;
  f.mx = m.m[0] * UT_POR_LSB_MAG;
  f.my = m.m[1] * UT_POR_LSB_MAG;
  f.mz = m.m[2] * UT_POR_LSB_MAG;
}

float moduloAceleracionG(const MuestraIMU &m) {
  const float x = m.a[0] / LSB_POR_G;
  const float y = m.a[1] / LSB_POR_G;
  const float z = m.a[2] / LSB_POR_G;
  return sqrtf(x * x + y * y + z * z);
}

uint32_t muestrasTotales() { return totalMuestras; }

uint32_t resincronizaciones() { return totalResincronizaciones; }

void llenarFila(FilaSCI &f) {
  // La fila SCI es a 1 Hz y actualizar() corre a 100 Hz: la fila lleva
  // la última muestra llegada desde la fila anterior. Después de copiarla
  // se marca usada, para no repetirla si no llega ninguna nueva (§4.2).
  if (!hayUltima) {
    return;  // celda vacía: no hubo muestra nueva desde la última fila
  }
  FilaIMU u;
  aFilaIMU(ultima, u);
  f.ax = u.ax;
  f.ay = u.ay;
  f.az = u.az;
  f.gx = u.gx;
  f.gy = u.gy;
  f.gz = u.gz;
  f.mx = u.mx;
  f.my = u.my;
  f.mz = u.mz;
  hayUltima = false;
}

}  // namespace SensorICM20948
