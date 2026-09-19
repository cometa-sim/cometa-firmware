// log_format.h
//
// Formato de los tres logs (REQUISITOS.md §4.2, §4.3, §4.5, §4.6),
// compartido por src/teensy y src/adalogger. Esto no acopla las dos
// cadenas entre sí (REQUISITOS.md §2: siguen sin compartir bus,
// alimentación ni lógica de sensores): son solo los nombres de columna,
// tipos y decimales fijados por el formato de archivo.
//
// Lista única (X-macro): cada log se define UNA vez como una lista de
// campos (nombre, tipo, decimales) — COMETA_SCI_CAMPOS / _IMU_ / _L2_.
// De esa lista única se generan, sin repetir nada a mano:
//   - la struct FilaSCI/FilaIMU/FilaL2 (un campo por columna),
//   - el encabezado CSV (escribirEncabezadoSCI/IMU/L2),
//   - la fila de datos (escribirFilaSCI/IMU/L2),
//   - limpiarFilaSCI/IMU/L2 (vaciar antes de cada tic),
//   - el número de columnas (COMETA_SCI_NUM_COLUMNAS, etc.), verificado
//     con un static_assert contra REQUISITOS.md §4.3/§4.5/§4.6.
//
// Tipos de campo (segundo argumento de cada X(...)):
//   U32  — uint32_t, siempre presente, nunca vacío (t_ms).
//   U32N — uint32_t "nullable": 0 = no hubo lectura nueva en esta fila
//          (dt_ms; una duración real de 0 ms no ocurre en la práctica,
//          es el mismo criterio de centinela evidente que se usa en
//          config_teensy.h/config_adalogger.h).
//   UTC  — char[21] con la hora ISO 8601; cadena vacía = celda vacía.
//   LL   — double (lat, lon); NAN = celda vacía.
//   F    — float; NAN = celda vacía.
// NAN = celda vacía es la regla general (REQUISITOS.md §4.2); utc y
// lat/lon son los casos aparte (cadena vacía y double en vez de float).

#ifndef COMETA_LOG_FORMAT_H
#define COMETA_LOG_FORMAT_H

#include <Arduino.h>
#include <SD.h>
#include <math.h>

// -----------------------------------------------------------------------
// Listas de campos — la única fuente de verdad de cada log.
// -----------------------------------------------------------------------

// SCI_nnn.CSV — datos científicos a 1 Hz (REQUISITOS.md §4.3). 57
// columnas: se agrega `reinit` al grupo Sistema, después de `i2c_recov`
// (REQUISITOS.md §1.3, §4.3).
//
// Decimales propuestos, a revisar (ver la descripción de la PR): según
// la resolución de cada sensor, no inventados sensor por sensor.
#define COMETA_SCI_CAMPOS(X) \
  X(t_ms,           U32,  0) \
  X(utc,            UTC,  0) \
  X(lat,            LL,   6) \
  X(lon,            LL,   6) \
  X(alt_m,          F,    1) \
  X(vz_ms,          F,    2) \
  X(vn_ms,          F,    2) \
  X(ve_ms,          F,    2) \
  X(sats,           F,    0) \
  X(pdop,           F,    1) \
  X(fix,            F,    0) \
  X(p_hPa,          F,    2) \
  X(t_ms8607_C,     F,    2) \
  X(rh_ms8607,      F,    1) \
  X(t_arm_C,        F,    2) \
  X(t_tubo_C,       F,    2) \
  X(err_arm,        F,    0) \
  X(err_tubo,       F,    0) \
  X(rh_sht,         F,    1) \
  X(t_sht_C,        F,    2) \
  X(co2_ppm,        F,    0) \
  X(t_scd_C,        F,    2) \
  X(p_inviata_hPa,  F,    0) \
  X(t_cassa_C,      F,    2) \
  X(t_pile_C,       F,    2) \
  X(t_centro_C,     F,    2) \
  X(t_scd_ds_C,     F,    2) \
  X(uva_raw,        F,    0) \
  X(uv_gain,        F,    0) \
  X(uv_res,         F,    0) \
  X(ax,             F,    4) \
  X(ay,             F,    4) \
  X(az,             F,    4) \
  X(gx,             F,    3) \
  X(gy,             F,    3) \
  X(gz,             F,    3) \
  X(mx,             F,    2) \
  X(my,             F,    2) \
  X(mz,             F,    2) \
  X(pms_on,         F,    0) \
  X(pm1,            F,    0) \
  X(pm25,           F,    0) \
  X(pm10,           F,    0) \
  X(n03,            F,    0) \
  X(n05,            F,    0) \
  X(n10,            F,    0) \
  X(cpi,            F,    0) \
  X(dt_ms,          U32N, 0) \
  X(q_pms,          F,    0) \
  X(q_rh,           F,    0) \
  X(q_arm,          F,    0) \
  X(q_tubo,         F,    0) \
  X(q_p,            F,    0) \
  X(v_batt,         F,    2) \
  X(i2c_recov,      F,    0) \
  X(reinit,         F,    0) \
  X(loop_ms,        F,    0)

// IMU_nnn.CSV — ventana de 100 Hz alrededor del estallido (REQUISITOS.md
// §4.6). 10 columnas.
#define COMETA_IMU_CAMPOS(X) \
  X(t_ms, U32, 0) \
  X(ax,   F,   4) \
  X(ay,   F,   4) \
  X(az,   F,   4) \
  X(gx,   F,   3) \
  X(gy,   F,   3) \
  X(gz,   F,   3) \
  X(mx,   F,   2) \
  X(my,   F,   2) \
  X(mz,   F,   2)

// L2_nnn.CSV — posición de respaldo a 1 Hz, Adalogger (REQUISITOS.md
// §4.5). 9 columnas.
#define COMETA_L2_CAMPOS(X) \
  X(t_ms,   U32, 0) \
  X(utc,    UTC, 0) \
  X(lat,    LL,  6) \
  X(lon,    LL,  6) \
  X(alt_m,  F,   1) \
  X(vz_ms,  F,   2) \
  X(sats,   F,   0) \
  X(fix,    F,   0) \
  X(v_batt, F,   2)

// -----------------------------------------------------------------------
// Despacho por tipo — el mecanismo que hace que utc y lat/lon (y U32/
// U32N) sean "casos aparte" sin duplicar cada macro por log.
// -----------------------------------------------------------------------

// Declaración del campo dentro de la struct.
#define COMETA_CAMPO_DECLARA_U32(nombre)  uint32_t nombre;
#define COMETA_CAMPO_DECLARA_U32N(nombre) uint32_t nombre;
#define COMETA_CAMPO_DECLARA_UTC(nombre)  char nombre[21];
#define COMETA_CAMPO_DECLARA_LL(nombre)   double nombre;
#define COMETA_CAMPO_DECLARA_F(nombre)    float nombre;
#define COMETA_CAMPO_DECLARA(nombre, tipo, decimales) \
  COMETA_CAMPO_DECLARA_##tipo(nombre)

// Vaciar el campo (limpiarFilaSCI/IMU/L2, al inicio de cada tic de
// 1000 ms). t_ms no se toca aquí: se fija aparte con millis() apenas se
// limpia la fila, siempre está presente.
#define COMETA_CAMPO_LIMPIA_U32(campo)
#define COMETA_CAMPO_LIMPIA_U32N(campo)  (campo) = 0
#define COMETA_CAMPO_LIMPIA_UTC(campo)   (campo)[0] = '\0'
#define COMETA_CAMPO_LIMPIA_LL(campo)    (campo) = NAN
#define COMETA_CAMPO_LIMPIA_F(campo)     (campo) = NAN
#define COMETA_CAMPO_LIMPIA(nombre, tipo, decimales) \
  COMETA_CAMPO_LIMPIA_##tipo(fila.nombre);

// Escribir el valor del campo (vacío si no hubo lectura: NAN en F/LL,
// 0 en U32N, cadena vacía en UTC; U32 se escribe siempre).
#define COMETA_CAMPO_ESCRIBE_U32(archivo, campo, decimales) \
  (archivo).print(campo)
#define COMETA_CAMPO_ESCRIBE_U32N(archivo, campo, decimales) \
  do { \
    if ((campo) != 0) (archivo).print(campo); \
  } while (0)
#define COMETA_CAMPO_ESCRIBE_UTC(archivo, campo, decimales) \
  (archivo).print(campo)
#define COMETA_CAMPO_ESCRIBE_LL(archivo, campo, decimales) \
  do { \
    if (!isnan(campo)) (archivo).print((campo), (decimales)); \
  } while (0)
#define COMETA_CAMPO_ESCRIBE_F(archivo, campo, decimales) \
  do { \
    if (!isnan(campo)) (archivo).print((campo), (decimales)); \
  } while (0)
#define COMETA_CAMPO_FILA(nombre, tipo, decimales) \
  COMETA_CAMPO_COMA(); \
  COMETA_CAMPO_ESCRIBE_##tipo(archivo, fila.nombre, decimales);

// Encabezado: cada campo se convierte en ",nombre"; la concatenación de
// literales adyacentes de C/C++ arma un único string en tiempo de
// compilación, y el +1 al imprimir descarta la coma inicial.
#define COMETA_CAMPO_HDR(nombre, tipo, decimales) "," #nombre

// Contar campos: cada uno se convierte en "+1" y se suman.
#define COMETA_CAMPO_CUENTA(nombre, tipo, decimales) +1

// -----------------------------------------------------------------------
// SCI
// -----------------------------------------------------------------------

struct FilaSCI {
  COMETA_SCI_CAMPOS(COMETA_CAMPO_DECLARA)
};

#define COMETA_SCI_NUM_COLUMNAS (0 COMETA_SCI_CAMPOS(COMETA_CAMPO_CUENTA))
static_assert(COMETA_SCI_NUM_COLUMNAS == 57,
              "SCI: el numero de columnas no coincide con REQUISITOS.md §4.3 "
              "(se esperan 57, con `reinit` despues de `i2c_recov`)");

// Deja la fila en su estado "sin lecturas": todos los F/LL en NAN, utc
// vacía, dt_ms en 0. t_ms no se toca (ver COMETA_CAMPO_LIMPIA_U32).
inline void limpiarFilaSCI(FilaSCI &fila) {
  COMETA_SCI_CAMPOS(COMETA_CAMPO_LIMPIA)
}

// Escribe el encabezado de SCI_nnn.CSV con los nombres de columna
// exactos de REQUISITOS.md §4.3.
inline void escribirEncabezadoSCI(File &archivo) {
  archivo.println(COMETA_SCI_CAMPOS(COMETA_CAMPO_HDR) + 1);
}

// Escribe una fila de SCI_nnn.CSV a partir de fila. Celda vacía = no
// hubo lectura en esa fila (REQUISITOS.md §4.2): no repite el último
// valor, no inventa ceros.
inline void escribirFilaSCI(File &archivo, const FilaSCI &fila) {
  bool primero = true;
#define COMETA_CAMPO_COMA() \
  do { \
    if (!primero) archivo.print(','); \
    primero = false; \
  } while (0)
  COMETA_SCI_CAMPOS(COMETA_CAMPO_FILA)
#undef COMETA_CAMPO_COMA
  archivo.println();
}

// -----------------------------------------------------------------------
// IMU
// -----------------------------------------------------------------------

struct FilaIMU {
  COMETA_IMU_CAMPOS(COMETA_CAMPO_DECLARA)
};

#define COMETA_IMU_NUM_COLUMNAS (0 COMETA_IMU_CAMPOS(COMETA_CAMPO_CUENTA))
static_assert(COMETA_IMU_NUM_COLUMNAS == 10,
              "IMU: el numero de columnas no coincide con REQUISITOS.md §4.6");

inline void limpiarFilaIMU(FilaIMU &fila) {
  COMETA_IMU_CAMPOS(COMETA_CAMPO_LIMPIA)
}

// Escribe el encabezado de IMU_nnn.CSV con los nombres de columna
// exactos de REQUISITOS.md §4.6.
inline void escribirEncabezadoIMU(File &archivo) {
  archivo.println(COMETA_IMU_CAMPOS(COMETA_CAMPO_HDR) + 1);
}

inline void escribirFilaIMU(File &archivo, const FilaIMU &fila) {
  bool primero = true;
#define COMETA_CAMPO_COMA() \
  do { \
    if (!primero) archivo.print(','); \
    primero = false; \
  } while (0)
  COMETA_IMU_CAMPOS(COMETA_CAMPO_FILA)
#undef COMETA_CAMPO_COMA
  archivo.println();
}

// -----------------------------------------------------------------------
// L2 (Adalogger)
// -----------------------------------------------------------------------

struct FilaL2 {
  COMETA_L2_CAMPOS(COMETA_CAMPO_DECLARA)
};

#define COMETA_L2_NUM_COLUMNAS (0 COMETA_L2_CAMPOS(COMETA_CAMPO_CUENTA))
static_assert(COMETA_L2_NUM_COLUMNAS == 9,
              "L2: el numero de columnas no coincide con REQUISITOS.md §4.5");

inline void limpiarFilaL2(FilaL2 &fila) {
  COMETA_L2_CAMPOS(COMETA_CAMPO_LIMPIA)
}

// Escribe el encabezado de L2_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.5.
inline void escribirEncabezadoL2(File &archivo) {
  archivo.println(COMETA_L2_CAMPOS(COMETA_CAMPO_HDR) + 1);
}

inline void escribirFilaL2(File &archivo, const FilaL2 &fila) {
  bool primero = true;
#define COMETA_CAMPO_COMA() \
  do { \
    if (!primero) archivo.print(','); \
    primero = false; \
  } while (0)
  COMETA_L2_CAMPOS(COMETA_CAMPO_FILA)
#undef COMETA_CAMPO_COMA
  archivo.println();
}

#endif  // COMETA_LOG_FORMAT_H
