// log_format.h
//
// Encabezados CSV de los tres logs (REQUISITOS.md §4.3, §4.5, §4.6),
// compartidos por src/teensy y src/adalogger. Esto no acopla las dos
// cadenas entre sí (REQUISITOS.md §2: siguen sin compartir bus,
// alimentación ni lógica de sensores): son solo los nombres de columna
// fijados por el formato de archivo, iguales estén donde estén escritos.

#ifndef COMETA_LOG_FORMAT_H
#define COMETA_LOG_FORMAT_H

#include <Arduino.h>
#include <SD.h>

// Escribe el encabezado de SCI_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.3. Usado solo por src/teensy.
inline void escribirEncabezadoSCI(File &archivo) {
  archivo.println(
      "t_ms,utc,"
      "lat,lon,alt_m,vz_ms,vn_ms,ve_ms,sats,pdop,fix,"
      "p_hPa,t_ms8607_C,rh_ms8607,"
      "t_arm_C,t_tubo_C,err_arm,err_tubo,"
      "rh_sht,t_sht_C,"
      "co2_ppm,t_scd_C,p_inviata_hPa,"
      "t_cassa_C,t_pile_C,t_centro_C,t_scd_ds_C,"
      "uva_raw,uv_gain,uv_res,"
      "ax,ay,az,gx,gy,gz,mx,my,mz,"
      "pms_on,pm1,pm25,pm10,n03,n05,n10,"
      "cpi,dt_ms,"
      "q_pms,q_rh,q_arm,q_tubo,q_p,"
      "v_batt,i2c_recov,loop_ms");
}

// Escribe el encabezado de IMU_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.6. Usado solo por src/teensy.
inline void escribirEncabezadoIMU(File &archivo) {
  archivo.println("t_ms,ax,ay,az,gx,gy,gz,mx,my,mz");
}

// Escribe el encabezado de L2_nnn.CSV con los nombres de columna exactos
// de REQUISITOS.md §4.5. Usado solo por src/adalogger.
inline void escribirEncabezadoL2(File &archivo) {
  archivo.println("t_ms,utc,lat,lon,alt_m,vz_ms,sats,fix,v_batt");
}

#endif  // COMETA_LOG_FORMAT_H
