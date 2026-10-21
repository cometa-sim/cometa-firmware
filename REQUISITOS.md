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

### 1.2 Módulos de sensor

Cada sensor del Teensy es un módulo en `src/teensy/sensores/` (un `.h`/`.cpp` por sensor; el MAX31865 es un solo archivo con dos instancias, A y B). Todos comparten la misma interfaz, un espacio de nombres por sensor con tres funciones:

```cpp
namespace SensorXxx {
  bool iniciar();                    // true si el sensor responde
  bool actualizar(uint32_t ahora);   // no bloqueante; true si la lectura de esta pasada fue válida
  void llenarFila(FilaSCI &f);       // toca solo sus propios campos, solo si hay un dato nuevo
}
```

`actualizar()` devuelve `bool` (no `void`): es la única forma de que el gestor de sensores (§1.3) sepa si una lectura falló, para contar los 5 fallos consecutivos.

`FilaSCI` (y `FilaIMU`, `FilaL2` para el Teensy y el Adalogger) son structs generadas en `include/log_format.h` a partir de una lista única de campos (nombre, tipo, decimales): de esa lista se generan también el encabezado CSV, la escritura de la fila y el número de columnas, para no repetir los nombres de columna en tres lugares distintos.

`sht45.h`/`sht45.cpp` es el módulo de ejemplo: es el único sensor completamente implementado, y el resto se escribe copiando esa forma. Los demás módulos son solo la declaración de la interfaz y cuerpos vacíos (`// TODO`, sin llamar a ninguna librería), con un comentario que remite a la sección de este documento que describe ese sensor.

**Código de arquitectura: no se toca.** Dos módulos traen, además de los cuerpos vacíos, código ya escrito que no es "lógica de sensor" sino una decisión de diseño de la que depende el vuelo. Está marcado en el archivo y no se modifica al completar el módulo:

- `geiger.cpp`: el contador `volatile` y la ISR (§5). Sin ISR conectada se pierden pulsos desde el primer instante, y sin `volatile` el compilador puede cachear el contador.
- `pms5003.cpp`: `apagar()` y `encender()`, la secuencia exacta de pines que evita la fuga de corriente por TX (§6). El orden de las líneas importa.

**Presupuesto de bloqueo.** `actualizar()` es "no bloqueante" con un límite concreto, no como principio vago:

- En el tic de **1000 ms**, una llamada a un módulo puede bloquear como mucho `COMETA_BLOQUEO_MAX_MS` = **20 ms** (`config_teensy.h`).
- En el tic de **10 ms** no se bloquea nada: es el del ICM-20948 y el buffer de la ventana IMU.
- Una espera más larga que esos 20 ms se parte en **arranque + recogida**: una pasada lanza la medición y se va, otra posterior la recoge cuando ya está lista, y las filas del medio llevan la celda vacía (§4.2), que es lo correcto — no se repite el último valor.
- El **SHT45** bloquea 10 ms (el `delay()` interno de `getEvent()` con `SHT4X_HIGH_PRECISION` y sin calefactor): entra en el presupuesto, por eso no se parte en dos pasadas. Con el calefactor encendido ese `delay()` pasa a 110 o 1100 ms y dejaría de ser conforme.

Estos 20 ms no descuadran la ventana IMU: el ICM-20948 se lee de su **FIFO**, no del registro instantáneo. La FIFO sigue llenándose a 100 Hz mientras el tic de 1000 ms trabaja, y el tic de 10 ms siguiente la vacía entera. Un retraso de 20 ms significa dos muestras que se leen juntas un poco después, no dos muestras perdidas. Lo que sí las perdería es bloquear **dentro** del tic de 10 ms, o dejar que la FIFO se desborde.

### 1.3 Sensores que no responden

Si `iniciar()` falla, el sensor queda **ausente**: sus celdas quedan vacías (§4.2) y el loop sigue con el resto. Se reintenta `iniciar()` cada 30 s (`COMETA_SENSOR_REINTENTO_MS`). Si el sensor ya estaba presente, 5 lecturas fallidas consecutivas de `actualizar()` (`COMETA_SENSOR_MAX_FALLOS`) lo marcan ausente con la misma regla de reintento.

Esta lógica vive en un único lugar (main.cpp, o un gestor común), no en cada módulo: el estudiante que escribe un sensor solo implementa `iniciar()`/`actualizar()`/`llenarFila()`.

El reintento se programa **relativo al instante del intento** (`ahora + COMETA_SENSOR_REINTENTO_MS`), no como un valor absoluto: si no, a partir del primer reintento la comparación queda siempre cierta y el sensor se reintenta en cada tic en vez de cada 30 s.

Dos sensores quedan **fuera** de esta máquina:

- El **ICM-20948**, porque se actualiza en el tic de 10 ms y no en el de 1000 ms (§1.1). Usa la misma lógica de ausencia, pero desde el tic rápido.
- El **Geiger**, porque no tiene con qué "no responder": es un pin con una interrupción, no un dispositivo de bus. `iniciar()` no puede fallar y `actualizar()` no puede devolver `false`. Si estuviera en la máquina, un `false` accidental lo marcaría ausente y dejaría de leerse el contador, perdiendo cuentas en silencio. `main.cpp` lo llama directo.

Los intentos de `iniciar()` (el primero al arrancar y cada reintento) se cuentan en la columna `reinit` de `SCI` (§4.3), que es la **suma de todos los sensores**: dice que algo se está reiniciando, no cuál. El desglose por sensor, junto con los ausentes al arranque, va en `META` (§4.7).

## 2. Las dos cadenas son independientes

| | Teensy 4.1 (nivel 3) | Feather M0 Adalogger (nivel 2) |
|---|---|---|
| Función | todos los sensores científicos | posición y altitud de respaldo, telemetría por radio |
| Sensores | ver §5 | un SAM-M8Q propio (I²C) |
| Radio | — | E220, LoRa 920 MHz, por `Serial1` (§2.2) |
| SD | `BUILTIN_SDCARD` | chip select pin 4 |
| Reloj | UTC del GPS | UTC del GPS |

No comparten bus, alimentación ni masa. El código de una placa no debe asumir nada de la otra: la única relación entre los dos logs es el **UTC**.

### 2.1 Alimentación

- El pack de baterías del Adalogger se conecta por el pin **USB** a través de un diodo (**1N5819** o **1N5817**), **nunca** por el conector **JST BAT**. Por eso el pin `A7` de fábrica del Feather M0 (pensado para medir la batería del JST) **no sirve** para medir este pack: hace falta un divisor propio en otro pin.
- Divisores de tensión de batería: **100 kΩ / 100 kΩ** en `A2` (Adalogger; no `A1`: en el Feather del banco el pin `A1` no hace contacto), **100 kΩ / 33 kΩ** en `A0` (Teensy).
  - Adalogger: el divisor va sobre el positivo del pack, **después** del interruptor y **antes** del diodo.
  - Teensy: el divisor va **después** del interruptor del nivel 3 (mide la tensión que realmente llega a la placa, no la del pack).
  - En los dos: un cerámico de **100 nF** entre el punto medio del divisor y GND.
- Adalogger: el SAM-M8Q y el E220 se alimentan desde el pin **3V** del Feather; masa común con el negativo del pack.
- Regla: el **interruptor del nivel 3 en OFF antes de enchufar el USB**.

### 2.2 Telemetría por radio (E220, solo Adalogger)

**Función.** La radio sirve para **encontrar la sonda y seguirla en tiempo real**, de forma independiente del Teensy. De ahí salen el contenido, el formato y el ritmo de la trama.

**Conexión y configuración del módulo**

- Módulo **E220**, LoRa **920 MHz**, por UART: `Serial1` del Feather, **TX (D1) → RXD** del módulo, **RX (D0) ← TXD** del módulo.
- **M0 y M1 a GND**: el módulo trabaja siempre en **modo normal (transparente)**. Lo que se escribe en `Serial1` sale por radio tal cual, y el firmware **no puede** cambiar la configuración del módulo.
- Por eso la configuración del E220 (canal para 920 MHz, velocidad UART, velocidad de aire 2,4 kbps, potencia, dirección) se hace **antes del montaje**, con el adaptador USB o con M0 = M1 en alto, y se anota. Valores: **[VERIFICAR]**. La velocidad de `Serial1` en el firmware (`COMETA_E220_BAUD`) tiene que coincidir con la que quedó guardada en el módulo.
- El pin **AUX no está conectado**: el firmware no sabe cuándo el módulo terminó de transmitir.

**Qué se transmite: la fila `L2`, y nada más**

Los datos científicos se quedan en el Teensy: mandarlos por radio alargaría la trama y ataría el nivel 2 al nivel 3, que es justo lo que la arquitectura evita (§2). La trama es la fila `L2` (§4.5) con dos cambios:

| Campo | Contenido |
|---|---|
| `id` | identificador fijo de la sonda (`COMETA_RADIO_ID`, `C2`): la distingue de otras transmisiones en el mismo canal |
| `n` | contador de tramas enviadas desde el arranque: los huecos dicen cuántas se perdieron, y una vuelta a 0 dice que la placa se reinició |
| `utc` | `hhmmss` (compacto), en vez del ISO 8601 del log |
| `lat`, `lon`, `alt_m`, `vz_ms`, `sats`, `fix`, `v_batt` | **igual que en `L2`**: mismos decimales (§4.5, `log_format.h`) y celda vacía cuando no hay dato (§4.2), para que en tierra un solo parser lea las dos fuentes |

`t_ms` no va: lo reemplazan `id` y `n`. Ejemplo:

```
C2,412,143210,-33.245678,-58.031234,18240.0,5.20,9,3,4.98
```

Sin fix: `C2,413,,,,,,4,0,4.98` (la trama sale igual: dice que la sonda está viva). Si el GPS no contestó en ese segundo, también `sats` y `fix` van vacíos: `C2,414,,,,,,,,4.98`.

Para qué sirve cada campo durante el seguimiento: `lat`, `lon` y `alt_m` dicen adónde ir; `vz_ms` dice en qué fase está el vuelo (positiva en subida, muy negativa justo después del estallido, unos pocos m/s en el descenso con paracaídas, cero en tierra); `sats` y `fix` dicen cuánto confiar en la posición; `v_batt` dice cuánto va a durar la transmisión.

**Cómo se transmite**

- **Una trama cada `COMETA_RADIO_PERIODO_MS` = 5 s**, durante todo el vuelo **y después del aterrizaje**. No se espacia: las pilas duran mucho más que el vuelo y la posición en tierra es el dato que más vale. Si el E220 interfiere con el Geiger, se sube a 10 s: **[VERIFICAR]** en banco, comparando `cpi` con la radio encendida y apagada.
- **También sin fix** (ver el ejemplo de arriba).
- **Texto CSV** terminado con `println()` (CR LF), sin espacios ni unidades: se lee en el monitor serie y se analiza fácil.
- **Sin checksum**: el E220 ya descarta los paquetes con CRC de radio incorrecto, así que en tierra llegan filas íntegras o no llega nada.
- **La trama se arma entera en un buffer** (`snprintf`) y se escribe con **una sola** llamada. En modo transparente el módulo corta el paquete cuando la UART queda en silencio: una fila escrita en pedazos puede salir partida en dos paquetes.
- **La radio nunca frena la SD.** En cada tic se escribe primero la fila en `L2_nnn.CSV` y después, si toca, la trama. La trama se manda sin esperar respuesta del módulo y no se reintenta. A 9600 baudios, 60 bytes tardan unos 60 ms en salir por la UART; si el buffer de salida de `Serial1` se llena, `write()` espera, por eso la trama tiene que ser corta.
- **Largo**: unos 60 bytes con CR LF; en el peor caso (contador de 5 cifras, `alt_m` > 10 km, `vz_ms` negativa de dos cifras), unos 64. A 2,4 kbps de aire eso ocupa el canal unos 0,3 s: entra con margen en 5 s.
- **Consumo**: depende de la potencia configurada en el módulo (**[VERIFICAR]**); el balance está en el manual.

**En tierra: cómo se lee la trama**

Lo que el receptor de tierra tiene que respetar para pasar las tramas a la página de la diretta junto con los puntos del SPOT Trace:

- **Una trama = una línea** terminada en CR LF. Se acepta solo si contiene `C2,` y tiene **10 campos** (9 comas); lo demás se descarta: en 920 MHz puede haber otros transmisores en el mismo canal. `C2,` se busca dentro de la línea, no solo en la posición 0: si el E220 de tierra tiene activado el byte de RSSI, ese byte binario llega después del CR LF y queda al principio de la línea siguiente.
- **Fecha.** La trama trae solo `hhmmss`; la fecha la pone el receptor con su reloj UTC. Si `hhmmss` queda más de 12 h por delante de la hora de recepción, la trama es del día anterior (recibida a las 00:00:02 con `235958`).
- **Celda vacía no es cero**: un punto va al mapa solo si `lat` y `lon` tienen valor. Una trama sin posición dice igual que la sonda está viva y cuánta batería le queda.
- **Varios receptores** reciben la misma trama: la clave para no duplicarla es `id` + fecha + `hhmmss`, el instante de la solución GPS, distinto en cada punto. `n` **no** sirve de clave: vuelve a 0 si la placa se reinicia y pisaría puntos anteriores del mismo vuelo.
- `n` sirve para medir el enlace: los huecos son tramas perdidas por ese receptor; una vuelta a 0 es un reinicio del Adalogger.
- La trama tiene los mismos números que la fila de `L2_nnn.CSV` del mismo segundo: después del vuelo, la telemetría recibida se compara con la SD uniendo por `utc`.

### 2.3 Fallas en el Adalogger

Ninguna falla detiene a las demás: sin SD sigue la radio; sin GPS siguen la SD y la radio (`t_ms`, `v_batt` y la trama sin posición).

- **GPS.** Como en el Teensy: solo UBX por I²C (`setI2COutput`) y NAV-PVT automático (`setAutoPVT`), así `getPVT()` no espera. Un tic sin NAV-PVT nuevo es normal; **3 s sin ninguno** (`COMETA_GPS_SIN_PVT_MAX_MS`) no: se recupera el bus (§3.3) y se reinicializa **enseguida**, reenviando el modo Airborne. Un SAM-M8Q que se reinició (una caída de tensión del pin 3V) vuelve con la configuración de fábrica, modo Portable, sin fix por encima de 12 km: es el único sensor de esta placa y no se espera al reintento. Si no contesta, o no manda ningún NAV-PVT después de reinicializado, se reintenta cada 30 s (`COMETA_GPS_REINTENTO_MS`). La recuperación del bus se hace antes de **cada** `begin()`, el del arranque incluido: un GPS que quedó a mitad de un byte cuando se reinició el Feather sostiene SDA baja, y sin eso no volvería nunca.
- **SD.** Si no monta al arrancar, o si una escritura falla en vuelo (`getWriteError()`), se cierra el archivo y se vuelve a montar cada 30 s (`COMETA_SD_REINTENTO_MS`), siguiendo en un `L2_nnn.CSV` nuevo (§4.1). El intento va al final del tic, después de la trama: `SD.begin()` sin tarjeta tarda hasta ~2 s.
- **LEDs**, para revisar la sonda antes de cerrarla sin conectar la PC: en cada tic destella uno, **verde** (pin 8, junto a la ranura de la SD) si la fila se escribió en la SD, **rojo** (pin 13) si la SD no está grabando.
- **Por Serial**, para el banco: el periodo real del watchdog y la causa del último reinicio (§3.11), y después la fila L2 de cada segundo (`L2: `) y cada trama enviada (`RADIO: `). Cada 10 s (`COMETA_ESTADO_PERIODO_MS`), una línea `ESTADO:` repite lo del arranque (GPS presente, modo Airborne confirmado, archivo de la SD, recuperaciones I²C, causa del reinicio): con el USB nativo del Feather M0 lo que se escribe en `setup()` sale antes de que el monitor serie se conecte y se pierde.

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
10. **Tensión de batería** por ADC en ambas placas: Teensy en `A0` (divisor 100 kΩ/33 kΩ), Adalogger en `A2` (divisor 100 kΩ/100 kΩ). Ver §2.1.

El termostato del pad calefactor **no** forma parte de la configuración de vuelo: no se implementa ahora.

### 3.11 Watchdog

- **Teensy**: `WDT_T4`, timeout 8 s (`COMETA_WATCHDOG_TIMEOUT_S`).
- **Adalogger**: `Adafruit_SleepyDog`, timeout 8 s (`COMETA_WATCHDOG_TIMEOUT_S`).
- Se arma **al principio de `setup()`**, antes de tocar nada, en las dos placas: así cubre también el arranque, que es donde más fácil es colgarse (una SD que no monta, un sensor que estira el clock del I²C para siempre).
- **En `setup()` se alimenta después de cada paso del arranque**: cada `iniciar()`, montar la SD, abrir el archivo, configurar el GPS. La regla es que **ningún paso suelto puede pasarse del timeout**, pero la suma de todos sí puede: once `iniciar()` con sus timeouts de I²C más el montaje de la SD se pasan de 8 s sin que nada esté roto. Sin estos *feed* la placa se reiniciaría antes de llegar a `loop()`, y otra vez, y otra: ciclo de reinicios infinito y ni una fila de log. Una sola llamada que se cuelgue de verdad sí reinicia, que es lo que se quiere.
- **En `loop()` se alimenta una sola vez por pasada**, al principio, y nunca dentro de un módulo de sensor: si un módulo se cuelga, el watchdog tiene que poder reiniciar la placa.
- `WDT_T4` no está en el registro de PlatformIO ni tiene releases de Git. Está **copiada en `lib/WDT_T4/`** (commit `1910b57d24468cd3419959118a2e1e6192f5e7fa`, MIT) en vez de bajarse por URL: así la compilación no depende de que GitHub responda ni de que el repositorio siga existiendo dentro de un año. El resto de las librerías sigue con versión fija en `lib_deps` (§1).
- El Adalogger **comprueba el periodo que devuelve `Watchdog.enable()`** y lo informa por Serial al arrancar: en el SAMD21 los periodos están cuantizados y la placa puede imponer uno distinto del pedido. Esta placa no tiene `META`, por eso va por Serial.
- Causa del último reinicio: se registra en `META` si el core la expone; si no, **[VERIFICAR]**.

## 4. Formato del log

### 4.1 Archivos

| Placa | Archivo | Contenido |
|---|---|---|
| Teensy | `SCI_nnn.CSV` | datos científicos a 1 Hz |
| Teensy | `IMU_nnn.CSV` | ventana de 100 Hz alrededor del estallido |
| Teensy | `META_nnn.TXT` | datos que no cambian durante el vuelo |
| Adalogger | `L2_nnn.CSV` | posición a 1 Hz |

- **Un archivo nuevo en cada arranque**, `nnn` progresivo (el primer número libre en la SD). Si la alimentación se corta, el archivo anterior queda cerrado e intacto. En el Adalogger, también cada vez que la SD vuelve después de un error (§2.3).
- Nombres 8.3 en mayúsculas: la microSD de 64 MB es **FAT16**.

### 4.2 Convenciones

- Separador **coma**, decimal **punto**, una fila de encabezado con los nombres de columna.
- **Celda vacía = no hubo lectura en esa fila.** No repetir el último valor (el SCD30, por ejemplo, actualiza cada 2 s).
- `t_ms`: milisegundos desde el arranque, siempre presente.
- `utc`: ISO 8601 (`2026-10-15T13:02:05Z`), vacío hasta tener fix. Se escribe **solo con `copiarUTC(f.utc, cadena)`** de `log_format.h`, nunca con `strcpy()` ni `sprintf()` directo sobre el campo: son 20 caracteres más el `\0`, justos, y una cadena más larga (un formato con milisegundos, una trama mal parseada) pisaría `lat` y `lon`, que están pegados en la struct — el log saldría con posiciones absurdas y sin ningún error visible. `copiarUTC()` acota el largo, garantiza el `\0` y trunca en vez de desbordar.
- `vz_ms` positiva hacia arriba.
- **Tipos de campo** (`include/log_format.h`): `t_ms` y `dt_ms` son `uint32_t`; `utc` es una cadena de hasta 20 caracteres; `lat` y `lon` son `double`; el resto de las columnas son `float`. **NAN = celda vacía** es la regla general para los campos numéricos; `utc` vacío es el mismo caso pero con cadena vacía en vez de NAN. `dt_ms`, al ser entero, usa 0 como su propio "vacío" (una duración real de 0 ms no ocurre en la práctica).

### 4.3 Columnas de `SCI`

| Grupo | Columnas |
|---|---|
| Tiempo | `t_ms`, `utc` |
| GPS | `lat`, `lon`, `alt_m`, `vz_ms`, `vn_ms`, `ve_ms`, `sats`, `pdop`, `fix` |
| MS8607 | `p_hPa`, `t_ms8607_C`, `rh_ms8607` |
| PT1000 | `t_ptA_C`, `t_ptB_C`, `err_ptA`, `err_ptB` (fault byte del MAX31865). A es la sonda exterior |
| SHT45 | `rh_sht`, `t_sht_C` |
| SCD30 | `co2_ppm`, `t_scd_C`, `p_inviata_hPa` |
| LTR390 | `uva_raw`, `uv_gain`, `uv_res` (conteos crudos) |
| ICM-20948 | `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz` |
| PMS5003 | `pms_on`, `pm1`, `pm25`, `pm10`, `n03`, `n05`, `n10` |
| Geiger | `cpi` (conteos en el intervalo), `dt_ms` (duración real del intervalo) |
| Calidad | `q_pms`, `q_rh`, `q_ptA`, `q_ptB`, `q_p` |
| Sistema | `v_batt`, `i2c_recov`, `reinit`, `loop_ms` |

`reinit` (después de `i2c_recov`): cuenta los intentos de `iniciar()` de todos los sensores, el primero al arrancar y cada reintento (§1.3). Es la suma; el desglose por sensor va en `META` (§4.7).

`loop_ms`: duración real de esa pasada del tic de 1000 ms, medida de punta a punta — todos los módulos más el armado de la fila, cerrada justo antes de escribirla. Es el número que dice si el presupuesto de bloqueo de §1.2 se está respetando en vuelo: si se acerca a 1000, el tic se empieza a atrasar y hay que mirar qué módulo bloquea de más.

53 columnas en total (`include/log_format.h`, `COMETA_SCI_NUM_COLUMNAS`). Los DS18B20 no vuelan: sus cuatro columnas se quitaron.

### 4.4 Flags de calidad (0 = en especificación, 1 = fuera)

| Flag | Se pone en 1 si |
|---|---|
| `q_pms` | T exterior (PT1000 A) < −10 °C |
| `q_rh` | T del SHT45 < −40 °C |
| `q_ptA`, `q_ptB` | T de esa sonda < −50 °C |
| `q_p` | `p_hPa` < 10 hPa |

### 4.5 Columnas de `L2` (Adalogger)

`t_ms`, `utc`, `lat`, `lon`, `alt_m`, `vz_ms`, `sats`, `fix`, `v_batt`

### 4.6 Columnas de `IMU`

`t_ms`, `ax`, `ay`, `az`, `gx`, `gy`, `gz`, `mx`, `my`, `mz`

### 4.7 Contenido de `META`

Hash de Git del firmware, frecuencia de reloj, `RREF` y `RNOMINAL`, resultado de la configuración Airborne, dirección detectada de cada dispositivo I²C, la causa del último reinicio (§3.11), y (si ocurre) el disparo de la ventana IMU: cuál criterio, `t_ms`, UTC, altitud.

Además, **una línea por sensor** con su nombre, si estaba presente al arranque y su contador propio de intentos de `iniciar()` (§1.3). La columna `reinit` de `SCI` dice que algo se reinició; este desglose dice cuál, que es la pregunta que se hace al analizar el vuelo.

## 5. Sensores y constantes

| Sensor | Bus | Dirección / pin | Notas |
|---|---|---|---|
| SCD30 | I²C | 0x61 | clock stretching largo |
| MS8607 | I²C | 0x76 + 0x40 | fondo de escala 10 hPa |
| SAM-M8Q | I²C (`Wire1`) | 0x42 | solo en su bus, ver abajo. En el Adalogger, otro 0x42 en otro bus |
| SHT45 | I²C | 0x44 | en el brazo exterior |
| ICM-20948 | I²C | **0x69** | breakout con AD0 alto, no 0x68. ±16 g, 100 Hz |
| LTR390 | I²C | 0x53 | |
| 2 × MAX31865 (PT1000 A y B) | SPI (A) y SPI1 (B) | A: MOSI **11**, MISO **12**, SCK **13**, CS **10**. B: MOSI1 **26**, MISO1 **39**, SCK1 **27**, CS **9** | 3 hilos. Un bus por placa: cada una cableada directo a sus pines. MISO1 en 39 porque el pin 1 es el TX1 del PMS5003 |
| PMS5003 | UART | `Serial1`: RX pin **0**, TX pin **1** | MOSFET en pin **4** |
| E220 (Adalogger) | UART | `Serial1`: TX (D1) → RXD, RX (D0) ← TXD | M0 = M1 = GND, ver §2.2 |
| GGreg20 | interrupción | vía optoacoplador, pin **3** | |

**I²C (Teensy).** SDA en pin **18**, SCL en pin **19** (bus por defecto de `Wire` en el Teensy 4.1) para todos los sensores I²C **menos el GPS**. El SAM-M8Q va **solo** en `Wire1`: SDA1 pin **17**, SCL1 pin **16**, sin ningún cable Qwiic hacia la cadena. En el banco, compartiendo el bus, dejaba SDA baja cada 1-3 min y se llevaba con él al ICM-20948 y al LTR390; en su propio bus, 19 min sin una sola recuperación (`COMETA_GPS_EN_WIRE1` en `config_teensy.h`).

**MAX31865 — atención.** Los ejemplos de Adafruit son para PT100. Nuestras sondas son **PT1000**:

```cpp
#define RREF      4300.0
#define RNOMINAL  1000.0
#define WIRING    MAX31865_3WIRE
```

Con `430` / `100` el sensor devuelve números creíbles y equivocados.

**Geiger.** La ISR solo incrementa un contador. Guardar los conteos **por intervalo** (`cpi`, `dt_ms`): sirven para el factor de Fano. La lectura y el azaramiento del contador van en un único bloque con interrupciones deshabilitadas, para no perder ni duplicar un pulso que llegue justo en el medio:

```cpp
noInterrupts();
uint32_t cuenta = contadorPulsos;
contadorPulsos = 0;
interrupts();
```

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
