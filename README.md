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
watchdog (`REQUISITOS.md` §1.1–§1.3, §3.11). **SHT45** es el módulo de
ejemplo. GPS, SCD30, MS8607, LTR390, MAX31865, DS18B20, ICM-20948,
Geiger y PMS5003 están implementados a partir del banco de prueba de los estudiantes
(`cometa-sim/COMETA-Teensy41`), corregidos para cumplir `REQUISITOS.md`;
cada archivo dice al principio qué viene del banco de prueba y qué se
cambió. Todos menos el MAX31865 del tubo y los DS18B20 se probaron en el
banco (entorno `banco`, ver abajo).

El `main.cpp` del **Teensy** está completo: log `SCI_nnn.CSV` y
`META_nnn.TXT` en la SD (probado en el banco), reloj a 150 MHz,
recuperación del bus I²C, batería en A0, flags de calidad, y la ventana
IMU del estallido con el ICM-20948 leído por FIFO (§7). La ventana se
prueba en tierra con el entorno `teensy41_prueba_imu`.

El **Adalogger** (`src/adalogger/main.cpp`) está completo: GPS SAM-M8Q en
modo Airborne con NAV-PVT automático (como el del Teensy), `L2_nnn.CSV`
nuevo en cada arranque con `flush()` cada 10 filas, batería en A1 y
trama de radio por el E220 cada 5 s (§2.2). Cada parte sigue sola si
otra falla: el GPS se reinicializa si deja de mandar datos, la SD se
vuelve a montar si una escritura falla, y dos LEDs dicen si está
grabando (§2.3). Cómo leer la trama en tierra: §2.2.

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
                          de ejemplo
  adalogger/main.cpp     setup()/loop() del Feather M0 Adalogger (nivel 2)
  banco/main.cpp         banco de prueba por USB: los mismos módulos de
                          teensy/sensores/, salida por Serial (no vuela)
lib/WDT_T4/              copia del watchdog del Teensy: no está en el
                          registro de PlatformIO (§3.11)
platformio.ini           entornos teensy41 y adalogger, librerías fijadas
```

| Entorno | Placa | Función | Archivos de log |
|---|---|---|---|
| `teensy41` | Teensy 4.1 | todos los sensores científicos | `SCI_nnn.CSV`, `IMU_nnn.CSV`, `META_nnn.TXT` |
| `adalogger` | Feather M0 Adalogger | posición y altitud de respaldo, telemetría E220 | `L2_nnn.CSV` |
| `banco` | Teensy 4.1 | prueba de sensores por USB, sin SD ni watchdog | ninguno: todo por Serial |
| `teensy41_prueba_imu` | Teensy 4.1 | el de vuelo, con la ventana IMU armada y disparada en tierra (no vuela) | los de `teensy41` |
| `adalogger_pines` | Feather M0 Adalogger | el Feather como voltímetro: tensión en A0-A5 y A7 por Serial, para revisar el divisor de batería sin multímetro (no vuela) | ninguno |

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
- qué frecuencia deja de verdad `set_arm_clock()` en el core instalado
  (§3.8): se lee en `META` como `cpu_hz`,
- si el core expone la **causa del último reinicio** para registrarla en
  `META`; si no la expone, queda documentado que no se registra (§3.11).

## Compilar

```sh
pio run                  # compila ambos entornos
pio run -e teensy41       # solo el Teensy 4.1
pio run -e adalogger       # solo el Feather M0 Adalogger
```

## Probar el Adalogger en el banco

El Adalogger no tiene un entorno aparte: el firmware de vuelo ya escribe
todo por USB.

```sh
pio run -e adalogger -t upload && pio device monitor -e adalogger
```

Al arrancar salen el periodo real del watchdog, la causa del reinicio,
el archivo de la SD (`SD: escribiendo en L2_nnn.CSV`) y el resultado del
modo Airborne, pero salen antes de que el monitor se conecte y casi
siempre se pierden: lo mismo se repite cada 10 s en una línea
`ESTADO: gps=si airborne=si sd=L2_nnn.CSV i2c_recov=0 reinicio=...`.
Además, una línea `L2: ...` por segundo (la misma fila que va a la SD)
y una `RADIO: ...` cada 5 s (la trama que sale por el E220). Sin PC: cada segundo destella el LED verde si la SD graba, el
rojo si no (`REQUISITOS.md` §2.3). Para ver las tramas por radio, un
segundo E220 con la misma configuración en un adaptador USB-serie, a
9600 baudios, en cualquier terminal serie.

Si `v_batt` no da la tensión del pack (por ejemplo ~2,7 V que salta,
igual con y sin pilas), el divisor no llega a `A1`. Sin multímetro, el
entorno `adalogger_pines` escribe cada segundo la tensión de A0-A5 y A7:
con el USB y el pack de 3 pilas conectados a la vez, al prender y apagar
el interruptor del pack el pin del divisor pasa de la mitad del pack a
0 V. Después se vuelve a cargar `adalogger`.

```sh
pio run -e adalogger_pines -t upload && pio device monitor -e adalogger_pines
```

## Probar los sensores desde el computador

El entorno `banco` arma el Teensy con los mismos módulos de
`src/teensy/sensores/` que el firmware de vuelo, pero sin SD, META ni
watchdog: al arrancar escanea el I²C y compara con las direcciones
esperadas, y cada segundo imprime por Serial qué sensores están presentes
y las celdas no vacías de la fila SCI. Un sensor que falta queda
AUSENTE y se reintenta cada 30 s, sin trabar a los demás: se puede
probar con solo una parte de los sensores conectados.

```sh
pio run -e banco -t upload
pio device monitor -e banco
```

Alimentación: solo el USB, **sin pilas** conectadas. El riel de 5 V de
la Perma-Proto (PMS5003 y GGreg20) normalmente viene del MPM3610: en el
banco se lo alimenta con un puente desde VIN del Teensy hasta la bornera
de entrada de la Perma-Proto, con el cable del MPM3610 desconectado de
esa bornera. Sacar el puente antes de volver a conectar las pilas.

## Licencia

MIT, ver [`LICENSE`](LICENSE).
