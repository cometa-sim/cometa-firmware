# COMETA · Firmware · Requisitos

Documento de referencia del repositorio `cometa-firmware`. Resume lo que el firmware **debe** hacer según el *Manual técnico de montaje* (cap. 7 y 8). Si algo de este archivo contradice un ejemplo de librería, **manda este archivo**.

Los valores marcados **[VERIFICAR]** no están confirmados: no se inventan, se miden o se consultan en la hoja de datos.

---

## 1. Estructura del proyecto

- Un solo proyecto PlatformIO con dos entornos en `platformio.ini`:
  - `teensy41` → código en `src/teensy/`
  - `adalogger` (`adafruit_feather_m0`) → código en `src/adalogger/`
  - separación con `build_src_filter`.
- Constantes en `include/config_teensy.h` y `include/config_adalogger.h`. **Ningún número mágico en el código.**
- Los encabezados CSV y el número de columnas de cada log están en `include/log_format.h`, compartido por las dos placas (son solo el formato de archivo, no acoplan la lógica de las dos cadenas).
- **Versiones de librerías fijas** en `lib_deps` (versión exacta, sin `^` ni `~`). No se actualizan entre la prueba en el congelador y el vuelo.
- La versión que vuela se marca con un tag de Git (`vuelo-1`).

### 1.1 Estructura del loop

`loop()` no usa `delay()`: dos tics no bloqueantes basados en `millis()`, reprogramados sumando el período (no fijándolos a `millis()` actual) para no acumular atraso.

- **Teensy**: tic de **10 ms** para el ICM-20948, el buffer circular y el disparo de la ventana IMU (§7); tic de **1000 ms** para el resto de los sensores y el log SCI.
- **Adalogger**: un solo tic de **1000 ms** para el log L2.

Si un tic se atrasa más de un período (por ejemplo, por el `flush()` de la SD o el clock stretching del SCD30 en el tic de 1000 ms), se realinea a `millis()` actual en vez de intentar recuperar el atraso disparando varias veces seguidas.

## 2. Las dos cadenas son independientes

| | Teensy 4.1 (nivel 3) | Feather M0 Adalogger (nivel 2) |
|---|---|---|
| Función | todos los sensores científicos | posición y altitud de respaldo |
| Sensores | ver §5 | un SAM-M8Q propio |
| SD | `BUILTIN_SDCARD` | chip select pin 4 |
| Reloj | UTC del GPS | UTC del GPS |

No comparten bus, alimentación ni masa. El código de una placa no debe asumir nada de la otra: la única relación entre los dos logs es el **UTC**.

### 2.1 Alimentación

- El pack de baterías del Adalogger se conecta por el pin **USB** a través de un diodo (**1N5819** o **1N5817**), **nunca** por el conector **JST BAT**. Por eso el pin `A7` de fábrica del Feather M0 (pensado para medir la batería del JST) **no sirve** para medir este pack: hace falta un divisor propio en otro pin.
- Divisores de tensión de batería: **100 kΩ / 100 kΩ** en `A1` (Adalogger), **100 kΩ / 33 kΩ** en `A0` (Teensy).
  - Adalogger: el divisor va sobre el positivo del pack, **antes** del diodo.
  - Teensy: el divisor va **después** del interruptor del nivel 3 (mide la tensión que realmente llega a la placa, no la del pack).
  - En los dos: un cerámico de **100 nF** entre el punto medio del divisor y GND.
- Regla: el **interruptor del nivel 3 en OFF antes de enchufar el USB**.

## 3. Requisitos del firmware (manual, cap. 7)

1. **GPS en modo Airborne <1g** en ambos receptores. En **cada arranque**: enviar `DYN_MODEL_AIRBORNE1g`, releer con `getDynamicModel()`, registrar el resultado en `META`. El SAM-M8Q es generación M8: usar los métodos legacy (CFG-NAV5), **no** `setVal`/VALSET. Guardado en flash o solo BBR: **[VERIFICAR]**.
2. **`flush()` cada 10 muestras.**
3. **Recuperación del bus I²C**: si SDA queda baja, 9 pulsos en SCL y reinicializar. Timeout **por encima** del clock stretching del SCD30 (3 ms es demasiado poco). Valor: **[VERIFICAR]** con la prueba 13. Contar las recuperaciones en `i2c_recov`.
4. **SCD30**: enviar la presión medida con `setAmbientPressure()` y registrar el valor **enviado** (`p_inviata_hPa`). La función acepta solo 700–1400 mbar: fuera de ese rango se envía el límite y la corrección se hace en post-procesamiento.
5. **SCD30**: `setAutoSelfCalibration(false)`. Calibración manual a valor conocido al aire libre.
6. **PMS5003** por MOSFET (ver §6).
7. **Datos fuera de especificación se marcan, no se descartan** (ver §4.4).
8. **Reloj del Teensy a 150 MHz** (rango aceptable 150–300). Frecuencias disponibles en el core instalado o `set_arm_clock()`: **[VERIFICAR]**.
9. **UTC del GPS** en ambos logs. Verificación con prueba en auto (prueba 12).
10. **Tensión de batería** por ADC en ambas placas: Teensy en `A0` (divisor 100 kΩ/33 kΩ), Adalogger en `A1` (divisor 100 kΩ/100 kΩ). Ver §2.1.

El termostato del pad calefactor **no** forma parte de la configuración de vuelo: no se implementa ahora.

## 4. Formato del log

### 4.1 Archivos

| Placa | Archivo | Contenido |
|---|---|---|
| Teensy | `SCI_nnn.CSV` | datos científicos a 1 Hz |
| Teensy | `IMU_nnn.CSV` | ventana de 100 Hz alrededor del estallido |
| Teensy | `META_nnn.TXT` | datos que no cambian durante el vuelo |
| Adalogger | `L2_nnn.CSV` | posición a 1 Hz |

- **Un archivo nuevo en cada arranque**, `nnn` progresivo (el primer número libre en la SD). Si la alimentación se corta, el archivo anterior queda cerrado e intacto.
- Nombres 8.3 en mayúsculas: la microSD de 64 MB es **FAT16**.

### 4.2 Convenciones

- Separador **coma**, decimal **punto**, una fila de encabezado con los nombres de columna.
- **Celda vacía = no hubo lectura en esa fila.** No repetir el último valor (el SCD30, por ejemplo, actualiza cada 2 s).
- `t_ms`: milisegundos desde el arranque, siempre presente.
- `utc`: ISO 8601 (`2026-10-15T13:02:05Z`), vacío hasta tener fix.
- `vz_ms` positiva hacia arriba.

### 4.3 Columnas de `SCI`

| Grupo | Columnas |
|---|---|
| Tiempo | `t_ms`, `utc` |
| GPS | `lat`, `lon`, `alt_m`, `vz_ms`, `vn_ms`, `ve_ms`, `sats`, `pdop`, `fix` |
| MS8607 | `p_hPa`, `t_ms8607_C`, `rh_ms8607` |
| PT1000 | `t_arm_C`, `t_tubo_C`, `err_arm`, `err_tubo` (fault byte del MAX31865) |
| SHT45 | `rh_sht`, `t_sht_C` |
| SCD30 | `co2_ppm`, `t_scd_C`, `p_inviata_hPa` |
| DS18B20 | `t_cassa_C`, `t_pile_C`, `t_centro_C`, `t_scd_ds_C` |
| LTR390 | `uva_raw`, `uv_gain`, `uv_res` (conteos crudos) |
| ICM-20948 | `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz` |
| PMS5003 | `pms_on`, `pm1`, `pm25`, `pm10`, `n03`, `n05`, `n10` |
| Geiger | `cpi` (conteos en el intervalo), `dt_ms` (duración real del intervalo) |
| Calidad | `q_pms`, `q_rh`, `q_arm`, `q_tubo`, `q_p` |
| Sistema | `v_batt`, `i2c_recov`, `loop_ms` |

### 4.4 Flags de calidad (0 = en especificación, 1 = fuera)

| Flag | Se pone en 1 si |
|---|---|
| `q_pms` | T exterior (PT1000 brazo) < −10 °C |
| `q_rh` | T del SHT45 < −40 °C |
| `q_arm`, `q_tubo` | T de esa sonda < −50 °C |
| `q_p` | `p_hPa` < 10 hPa |

### 4.5 Columnas de `L2` (Adalogger)

`t_ms`, `utc`, `lat`, `lon`, `alt_m`, `vz_ms`, `sats`, `fix`, `v_batt`

### 4.6 Columnas de `IMU`

`t_ms`, `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz`

### 4.7 Contenido de `META`

Hash de Git del firmware, frecuencia de reloj, `RREF` y `RNOMINAL`, ROM de cada DS18B20 con su posición, resultado de la configuración Airborne, dirección detectada de cada dispositivo I²C, y (si ocurre) el disparo de la ventana IMU: cuál criterio, `t_ms`, UTC, altitud.

## 5. Sensores y constantes

| Sensor | Bus | Dirección / pin | Notas |
|---|---|---|---|
| SCD30 | I²C | 0x61 | clock stretching largo |
| MS8607 | I²C | 0x76 + 0x40 | fondo de escala 10 hPa |
| SAM-M8Q | I²C | 0x42 | en el Adalogger, otro 0x42 en otro bus |
| SHT45 | I²C | 0x44 | en el brazo exterior |
| ICM-20948 | I²C | **0x69** | breakout con AD0 alto, no 0x68. ±16 g, 100 Hz |
| LTR390 | I²C | 0x53 | |
| 2 × MAX31865 | SPI | CS: brazo exterior pin **10**, tubo pin **9** | 3 hilos |
| PMS5003 | UART | `Serial1`: RX pin **0**, TX pin **1** | MOSFET en pin **4** |
| 4 × DS18B20 | 1-Wire | pin **2**, pull-up 4,7 kΩ | |
| GGreg20 | interrupción | vía optoacoplador, pin **3** | |

**I²C (Teensy).** SDA en pin **18**, SCL en pin **19** (bus por defecto de `Wire` en el Teensy 4.1).

**MAX31865 — atención.** Los ejemplos de Adafruit son para PT100. Nuestras sondas son **PT1000**:

```cpp
#define RREF      4300.0
#define RNOMINAL  1000.0
#define WIRING    MAX31865_3WIRE
```

Con `430` / `100` el sensor devuelve números creíbles y equivocados.

**DS18B20.** Identificar las cuatro por ROM antes del montaje (caja, pilas, centro del volumen, SCD30). Resolución 12 bits con `setWaitForConversion(false)`: nunca bloquear el loop 750 ms.

**Geiger.** La ISR solo incrementa un contador. Guardar los conteos **por intervalo** (`cpi`, `dt_ms`): sirven para el factor de Fano.

## 6. Control del PMS5003

- **Siempre apagado por encima de 5 km** de altitud GPS.
- Por debajo de 5 km, en subida: se apaga si T exterior < −15 °C, se vuelve a encender si T > −12 °C (histéresis 3 K). Si la T no es válida, decide solo el techo de 5 km.
- En bajada (después del estallido detectado, §7): se vuelve a encender por debajo de 5 km.
- `pms_on` se registra **siempre**: sin eso, en el análisis no se distingue un cero de un sensor apagado.
- **Corte real, sin fuga por TX.** El MOSFET (canal N, ya en el esquema) corta la **masa** del PMS5003, no el positivo; se mantiene porque el corte real permite reiniciar un sensor colgado, algo que en vuelo no se puede hacer a mano. Pero con la masa cortada, si `Serial1` sigue activa, su línea TX en reposo alto inyecta corriente (hasta 8 mA) por los diodos de protección del PMS y lo alimenta a medias — parece apagado y no lo está, hasta 34 mAh en 4 h (~4 % del balance) y riesgo de dejarlo en un estado indefinido. Por eso, al apagar: primero `Serial1.end()` y las líneas RX/TX en alta impedancia (`INPUT`), recién después el MOSFET a `LOW`. Al encender: primero el MOSFET a `HIGH`, después `Serial1.begin()`.
- Tras cada encendido, descartar las primeras **30 s** de lecturas: el ventilador tarda en estabilizarse.

## 7. Ventana IMU alrededor del estallido

- **Buffer circular en RAM siempre activo**, 100 Hz, en binario, de al menos **30 s**. No se escribe en la SD hasta el disparo.
- La IMU se lee desde la **FIFO interna** del ICM-20948 (que muestrea a 100 Hz por su cuenta): el tic de 10 ms (§1.1) vacía todas las muestras presentes en la FIFO en cada pasada, así un retraso del tic de 1000 ms (clock stretching del SCD30, GPS, `flush()` de la SD) no pierde muestras de la IMU. Cada muestra conserva su propio `t_ms`.
- **Armado**: altitud GPS > 10 km, **o** `p_hPa` < 250 si el GPS no es válido.
- **Disparo = el primero de los dos**:
  1. caída libre: |a| < 0,3 g sostenido durante ≥ 1 s;
  2. respaldo GPS: `vz_ms` < −10 durante 3 fixes consecutivos.
- **Ventana guardada**: 20 s antes y 90 s después del disparo.
- **Una sola ventana**, sin rearmado. Criterio, `t_ms`, UTC y altitud en `META`.
- Umbrales en `config_teensy.h`, para poder probarlos en tierra.

## 8. Orden de trabajo

Primero las pruebas del manual que dependen solo del firmware, cada sensor por separado:

1. **Prueba 13** — SCD30 solo en el Teensy, una hora de lecturas: medir el peor caso de clock stretching y fijar el timeout I²C.
2. **Prueba 1** — modo Airborne en ambos SAM-M8Q, con u-center clásico: leer, apagar, encender, releer.
3. **Prueba 3** — cortar la alimentación 20 veces al azar durante la grabación: el archivo debe ser siempre legible.
4. MicroSD de 64 MB en FAT16 montada y escrita en **ambas** placas.

Después se integra el resto, **de a un sensor por vez**, con un commit y una prueba por sensor. Al final: **prueba 6** (escáner I²C con la sonda cerrada) y **prueba 12** (sincronía de relojes en auto).
