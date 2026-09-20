# cometa-firmware

Firmware del globo estratosférico estudiantil COMETA (Scuola Italiana di
Montevideo): logger científico Teensy 4.1 y logger de respaldo Feather M0
Adalogger.

La especificación vinculante es [`REQUISITOS.md`](REQUISITOS.md): resume lo
que el firmware **debe** hacer según el *Manual técnico de montaje* (cap. 7
y 8). Si algo de este repositorio contradice un ejemplo de librería, manda
`REQUISITOS.md`.

## Estado del proyecto

El contrato del firmware ya está cerrado: formato de log, interfaz de los
módulos de sensor, presupuesto de bloqueo, manejo de sensores ausentes y
watchdog (`REQUISITOS.md` §1.1–§1.3, §3.11). De los sensores, solo
**SHT45** está implementado de verdad, como módulo de ejemplo; el resto
de `src/teensy/sensores/` son cuerpos vacíos con un comentario en
español que indica el requisito de `REQUISITOS.md` al que corresponde,
listos para que cada estudiante complete el suyo.

Dos cosas antes de escribir un módulo:

- **El presupuesto de bloqueo** (§1.2): en el tic de 1000 ms una llamada
  puede bloquear como mucho `COMETA_BLOQUEO_MAX_MS` = 20 ms; en el de
  10 ms, nada. Una espera más larga se parte en arranque y recogida
  (el caso del DS18B20, 750 ms de conversión).
- **El código marcado "no tocar"** en `geiger.cpp` y `pms5003.cpp`
  (§1.2): la ISR del contador y la secuencia de apagado del PMS5003 ya
  están escritas y no son parte del ejercicio.

## Estructura del proyecto

Un solo proyecto PlatformIO con dos entornos independientes
(`REQUISITOS.md` §1, §2). Las dos cadenas no comparten bus, alimentación
ni masa; la única relación entre los dos logs es el UTC del GPS.

```
include/
  config_teensy.h       constantes de la cadena científica (Teensy 4.1)
  config_adalogger.h    constantes de la cadena de respaldo (Feather M0)
  log_format.h           FilaSCI/FilaIMU/FilaL2, encabezados y filas CSV,
                          generados desde una lista única de campos, y
                          copiarUTC() (REQUISITOS.md §1.2, §4.2)
src/
  teensy/main.cpp        setup()/loop(), gestor de sensores (§1.3), watchdog
  teensy/sensores/       un módulo (.h/.cpp) por sensor (§1.2); sht45 es el
                          único implementado de verdad
  adalogger/main.cpp     setup()/loop() del Feather M0 Adalogger (nivel 2)
lib/WDT_T4/              copia del watchdog del Teensy: no está en el
                          registro de PlatformIO (§3.11)
platformio.ini           entornos teensy41 y adalogger, librerías fijadas
```

| Entorno | Placa | Función | Archivos de log |
|---|---|---|---|
| `teensy41` | Teensy 4.1 | todos los sensores científicos | `SCI_nnn.CSV`, `IMU_nnn.CSV`, `META_nnn.TXT` |
| `adalogger` | Feather M0 Adalogger | posición y altitud de respaldo | `L2_nnn.CSV` |

## Valores pendientes de verificar

La mayoría de los pines y divisores de `REQUISITOS.md` §2.1 y §5 ya están
decididos y fijados en `config_teensy.h`/`config_adalogger.h`. Quedan
marcadas **[VERIFICAR]** solo dos constantes: no se inventan, se miden
antes del vuelo. `verificarConfiguracion()` (llamada desde `setup()` en
ambas placas) las controla en cada arranque y avisa si siguen sin
confirmar (en `META` en el Teensy, por Serial en el Adalogger):

- el timeout de recuperación I²C (`COMETA_I2C_TIMEOUT_MS`): en el Teensy
  con la **prueba 13** (peor caso de clock stretching del SCD30); en el
  Adalogger alcanza con medirlo en banco, ya que en ese bus solo está el
  GPS,
- identificar por ROM las cuatro sondas DS18B20 tras el montaje (caja,
  pilas, centro, SCD30).

Aparte hay tres ítems que no dependen de un pin ni un divisor y siguen
sin resolver porque tampoco se pueden fijar de antemano en una
constante:

- si `DYN_MODEL_AIRBORNE1g` se guarda en flash o solo en BBR (§3.1),
- qué frecuencias acepta `set_arm_clock()` en el core instalado (§3.8),
- si el core expone la **causa del último reinicio** para registrarla en
  `META`; si no la expone, queda documentado que no se registra (§3.11).

## Compilar

```sh
pio run                  # compila ambos entornos
pio run -e teensy41       # solo el Teensy 4.1
pio run -e adalogger       # solo el Feather M0 Adalogger
```

## Licencia

MIT, ver [`LICENSE`](LICENSE).
