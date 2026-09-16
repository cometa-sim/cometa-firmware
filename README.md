# cometa-firmware

Firmware del globo estratosférico estudiantil COMETA (Scuola Italiana di
Montevideo): logger científico Teensy 4.1 y logger de respaldo Feather M0
Adalogger.

La especificación vinculante es [`REQUISITOS.md`](REQUISITOS.md): resume lo
que el firmware **debe** hacer según el *Manual técnico de montaje* (cap. 7
y 8). Si algo de este repositorio contradice un ejemplo de librería, manda
`REQUISITOS.md`.

## Estado del proyecto

Este repositorio contiene por ahora solo la **impalcatura** del firmware:
estructura del proyecto, configuración de PlatformIO, constantes y las
firmas de las funciones de cada sensor. La lógica de cada sensor todavía
**no** está implementada (son funciones vacías con un comentario en
español que indica el requisito de `REQUISITOS.md` al que corresponde). La
única lógica real ya escrita es la de las cabeceras CSV (`§4`).

## Estructura del proyecto

Un solo proyecto PlatformIO con dos entornos independientes
(`REQUISITOS.md` §1, §2). Las dos cadenas no comparten bus, alimentación
ni masa; la única relación entre los dos logs es el UTC del GPS.

```
include/
  config_teensy.h      constantes de la cadena científica (Teensy 4.1)
  config_adalogger.h   constantes de la cadena de respaldo (Feather M0)
src/
  teensy/main.cpp      setup()/loop() del Teensy 4.1 (nivel 3)
  adalogger/main.cpp   setup()/loop() del Feather M0 Adalogger (nivel 2)
platformio.ini         entornos teensy41 y adalogger, librerías fijadas
```

| Entorno | Placa | Función | Archivos de log |
|---|---|---|---|
| `teensy41` | Teensy 4.1 | todos los sensores científicos | `SCI_nnn.CSV`, `IMU_nnn.CSV`, `META_nnn.TXT` |
| `adalogger` | Feather M0 Adalogger | posición y altitud de respaldo | `L2_nnn.CSV` |

## Valores pendientes de verificar

Varias constantes de `REQUISITOS.md` están marcadas **[VERIFICAR]**: no se
inventan, se miden o se consultan en la hoja de datos antes del vuelo.
En el código aparecen con un centinela evidente (por ejemplo `-1` para un
pin) y un comentario `// TODO VERIFICAR`. Antes de volar hay que:

- confirmar el timeout de recuperación I²C con la **prueba 13**,
- medir el peor caso de clock stretching del SCD30,
- fijar los pines de los dos MAX31865, del bus 1-Wire, del Geiger, del
  MOSFET del PMS5003 y del divisor de batería en ambas placas,
- identificar por ROM las cuatro sondas DS18B20 tras el montaje,
- confirmar el modo de cableado del MAX31865 (2/3/4 hilos) con los
  puentes soldados en la placa,
- confirmar si `DYN_MODEL_AIRBORNE1g` se guarda en flash o solo en BBR.

## Compilar

```sh
pio run                  # compila ambos entornos
pio run -e teensy41       # solo el Teensy 4.1
pio run -e adalogger       # solo el Feather M0 Adalogger
```

## Licencia

MIT, ver [`LICENSE`](LICENSE).
