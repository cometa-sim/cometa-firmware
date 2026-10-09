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
// PENDIENTE (§1.2, §7): vaciar la FIFO del chip en vez de leer el
// registro de la última muestra. Leyendo el registro, un tic de 10 ms
// atrasado pierde las muestras intermedias; la ventana del estallido las
// necesita. actualizar() ya está en el tic de 10 ms y deja la última
// muestra para llenarFila(), pero la FIFO y la entrega de cada muestra
// al buffer circular (actualizarBufferIMU() en main.cpp) quedan por
// hacer.

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

float ax = NAN, ay = NAN, az = NAN;
float gx = NAN, gy = NAN, gz = NAN;
float mx = NAN, my = NAN, mz = NAN;

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

  return icm.status == ICM_20948_Stat_Ok;
}

bool actualizar(uint32_t ahora) {
  (void)ahora;
  if (!icm.dataReady()) {
    // A 100 Hz casi siempre hay dato; si no, no es un fallo del bus.
    return icm.status == ICM_20948_Stat_Ok || icm.status == ICM_20948_Stat_NoData;
  }
  icm.getAGMT();
  if (icm.status != ICM_20948_Stat_Ok) {
    return false;
  }
  ax = icm.accX() / 1000.0f;  // mg -> g
  ay = icm.accY() / 1000.0f;
  az = icm.accZ() / 1000.0f;
  gx = icm.gyrX();            // °/s
  gy = icm.gyrY();
  gz = icm.gyrZ();
  mx = icm.magX();            // µT
  my = icm.magY();
  mz = icm.magZ();
  return true;
}

void llenarFila(FilaSCI &f) {
  // La fila SCI es a 1 Hz y actualizar() corre a 100 Hz: la fila lleva
  // la última muestra llegada desde la fila anterior. Después de copiarla
  // se vacía, para no repetirla si no llega ninguna nueva (§4.2).
  if (isnan(ax)) {
    return;  // celda vacía: no hubo muestra nueva desde la última fila
  }
  f.ax = ax;
  f.ay = ay;
  f.az = az;
  f.gx = gx;
  f.gy = gy;
  f.gz = gz;
  f.mx = mx;
  f.my = my;
  f.mz = mz;
  ax = ay = az = gx = gy = gz = mx = my = mz = NAN;
}

}  // namespace SensorICM20948
