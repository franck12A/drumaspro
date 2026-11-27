# Cadena de efectos (Etapa 5A)

Seis slots en serie después del bus de voces y antes de la ganancia maestra y el
limitador. Cada slot es un efecto estéreo, 100 % wet: la cadena aplica bypass,
mezcla dry/wet y compensación de latencia. Los efectos no conocen el motor ni la
consola.

```
voces -> slots 1..6 (Reverb, Delay, Chorus, Drive, Filter, Filter)
      -> ganancia maestra -> limitador look-ahead -> safety clip -> dispositivo
```

Los seis slots arrancan **bypass**, de modo que el sonido por defecto sigue
siendo el de la Etapa 4. La consola permite activarlos uno a uno.

## Tipos e ids estables

| Tipo | ID | Implementación |
|---|---|---|
| Reverb | `0xB7E55817` (`fx.reverb`) | DSPark `AlgorithmicReverb` (FDN 16 líneas, calidad Full), mix interno a 1 |
| Delay | `0x75FD7016` (`fx.delay`) | Propia, estéreo, Hermite y cross-feedback ping-pong |
| Chorus | `0x90665BCD` (`fx.chorus`) | DSPark `Chorus` (4 voces), mix interno a 1 |
| Drive | `0x09D826CD` (`fx.drive`) | Propia, 2× con `dspark::Oversampling`, SVF pre/post y compensación RMS |
| Filter | `0x9C70CE01` (`fx.filter`) | `dspark::StateVariableFilter`, LP/HP/BP |
| Flanger | `fx.flanger` | Propia, delay fraccional modulado con feedback |
| Phaser | `fx.phaser` | Propia, 4 all-pass de primer orden barridos por LFO |
| Tremolo | `fx.tremolo` | Propia, LFO de amplitud con forma y offset estéreo |
| Vibrato | `fx.vibrato` | Propia, delay corto modulado (wow de pitch) |
| EQ | `fx.eq` | Propia, 3 bandas RBJ: shelf grave, campana media, shelf agudo |
| Lo-Fi | `fx.lofi` | Propia: bit-depth, decimación, ruido de vinilo y wow lento |
| Limitador maestro | `0xBD734635` (`fx.limiter`) | DSPark `Limiter`, sólo etapa maestra (no ocupa slot) |

La lista de tipos vive en un solo sitio, `fx::kEffectTypeCycle`
(`EffectTypes.h`): la fábrica, el ciclo de la consola, `EngineBridge` y el
selector de la UI la leen de ahí. `isKnownEffectType` recorre esa lista y un test
falla si un tipo del ciclo no lo puede construir la fábrica o si su nombre no
coincide.

## Parámetros

Cada efecto tiene su propio `core::ParameterRegistry`; los ids son FNV-1a del
nombre y no cambian. La mezcla dry/wet y el bypass pertenecen al slot, no al
efecto.

| ID (hex) | Nombre | Rango | Unidad | Curva | Defecto |
|---|---|---|---|---|---|
| `0x0D97A9F1` | `reverb.decay` | 0.2..20 | s | logarítmica | 2.0 |
| `0xD946F680` | `reverb.size` | 0.01..1 | proporción | lineal | 0.6 |
| `0x2E170E09` | `reverb.damping` | 0..1 | proporción | lineal | 0.35 |
| `0xDF851B35` | `reverb.preDelay` | 0..200 | ms | lineal | 20 |
| `0x283B0AE3` | `delay.time` | 10..2000 | ms | logarítmica | 350 |
| `0x64D89CD9` | `delay.feedback` | 0..0.95 | proporción | lineal | 0.35 |
| `0x17CCCDBA` | `delay.tone` | 200..16000 | Hz | logarítmica | 6000 |
| `0xBB9E9490` | `delay.pingPong` | 0..1 | proporción | lineal | 1.0 |
| `0xE5150B8D` | `chorus.rate` | 0.05..5 | Hz | logarítmica | 0.5 |
| `0xA0688260` | `chorus.depth` | 0.5..20 | ms | lineal | 6 |
| `0xF4B75098` | `chorus.voices` | 1..4 | voces | lineal | 3 |
| `0x4543E798` | `chorus.spread` | 0..1 | proporción | lineal | 0.7 |
| `0x35651A0F` | `drive.amount` | 0..1 | proporción | lineal | 0.35 |
| `0xE78ED877` | `drive.tone` | 500..16000 | Hz | logarítmica | 7000 |
| `0xAB605306` | `drive.output` | −12..12 | dB | lineal | 0 |
| `0xBF58F22E` | `filter.cutoff` | 20..20000 | Hz | logarítmica | 4000 |
| `0x57213419` | `filter.resonance` | 0..1 | proporción | lineal | 0.15 |
| `0x0E267C44` | `filter.mode` | 0..2 | — (LP/HP/BP) | lineal | 0 |
| `flanger.rate` | 0.05..5 | Hz | logarítmica | 0.25 |
| `flanger.depth` | 0..8 | ms | lineal | 2.0 |
| `flanger.feedback` | 0..0.9 | proporción | lineal | 0.4 |
| `flanger.delay` | 0.5..10 | ms | lineal | 2.5 |
| `phaser.rate` | 0.05..5 | Hz | logarítmica | 0.3 |
| `phaser.depth` | 0..1 | proporción | lineal | 0.7 |
| `phaser.feedback` | 0..0.9 | proporción | lineal | 0.3 |
| `phaser.frequency` | 100..4000 | Hz | logarítmica | 700 |
| `tremolo.rate` | 0.05..20 | Hz | logarítmica | 5 |
| `tremolo.depth` | 0..1 | proporción | lineal | 0.5 |
| `tremolo.shape` | 0..2 | — (seno/tri/cuadrada) | lineal | 0 |
| `tremolo.stereo` | 0..1 | proporción | lineal | 0 |
| `vibrato.rate` | 0.05..10 | Hz | logarítmica | 5 |
| `vibrato.depth` | 0..5 | ms | lineal | 1.5 |
| `vibrato.delay` | 5..20 | ms | lineal | 8 |
| `eq.low` | −12..12 | dB | lineal | 0 |
| `eq.mid` | −12..12 | dB | lineal | 0 |
| `eq.high` | −12..12 | dB | lineal | 0 |
| `eq.midFrequency` | 200..5000 | Hz | logarítmica | 1000 |
| `lofi.bits` | 4..16 | bits | lineal | 10 |
| `lofi.rate` | 1000..48000 | Hz | logarítmica | 24000 |
| `lofi.noise` | 0..1 | proporción | lineal | 0.08 |
| `lofi.wow` | 0..1 | proporción | lineal | 0.2 |

El delay reserva `ChainDescription::tempoBpm` para la sincronía por BPM futura;
en esta etapa el tiempo se expresa en milisegundos y no hay división musical.

## Efectos de la Etapa 5B

Los seis viven en `src/fx/ExtraEffects.h`, sin dependencia nueva, con delay
circular de tamaño potencia de dos e interpolación lineal, y sin asignaciones
tras `prepare()`. Todos reportan latencia 0 salvo el limitador maestro; sus
colas (de 0 a 150 ms) solo importan para la política de bypass.

- **Flanger**: delay de 0.5–10 ms barrido por un LFO de 0.05–5 Hz con
  realimentación 0–0.9; las dos canales van desfasadas un cuarto de vuelta para
  abrir el peine. Mezcla interna dry + wet al 0.7 para no ganar nivel.
- **Phaser**: cuatro all-pass de primer orden por canal; el coeficiente se
  recalcula una vez por bloque (5 ms) a partir del LFO y de `phaser.frequency`,
  mucho más barato que un `tan()` por muestra y sin escalones audibles a esas
  velocidades de barrido.
- **Tremolo**: `depth = 0` deja ganancia unidad y `depth = 1` baja a silencio;
  la forma (seno/triángulo/cuadrada) y el offset estéreo permiten paneo
  rítmico. Profundidad y offset van suavizados.
- **Vibrato**: delay de 5–20 ms con ±0–5 ms de modulación, leído con
  interpolación lineal; es un efecto 100 % wet, así que la latencia que reporta
  es 0 y la cola es el delay base.
- **EQ**: shelves RBJ de 200 Hz y 4 kHz más una campana de Q 0.9 en
  `eq.midFrequency`. Las ganancias se mueven con un one-pole por bloque (~30 ms
  hasta el valor nuevo) y los coeficientes solo se recalculan cuando cambian,
  no por muestra.
- **Lo-Fi**: cuantización a `lofi.bits` (4–16), *sample and hold* a
  `lofi.rate`, ruido blanco de vinilo y un wow lento (LFO de 0.4 Hz sobre un
delay de 2 ms). La decimación **aliasea a propósito**: es el carácter del
efecto, no un defecto del oversampling; el limitador maestro sigue acotando la
salida. El método se eligió así porque un antialias previo a la decimación
borraría justo el grano que se busca, y porque no añade dependencias ni memoria
dinámica. La medida de alias de este efecto queda pendiente de una prueba FFT
propia (ver deuda en `SESSION_STATUS.md`).

## Controles

En la consola:

| Tecla | Acción |
|---|---|
| `Q W E R T Y` | seleccionar slot 1..6 y pasar a edición de efectos |
| `B` | conmutar bypass del slot seleccionado |
| `N` | ciclar el tipo del slot (reverb, delay, chorus, drive, filter) |
| `1`/`2`/`3` | seleccionar parámetro del efecto |
| `4` | seleccionar la mezcla dry/wet |
| `Tab` | alternar foco sintetizador/efectos |
| `↑`/`↓` o `+`/`-` | ajustar el parámetro seleccionado |
| `0` | volver a los parámetros del sintetizador |

En la UI Qt/QML, cada slot muestra su tipo, bypass y mezcla; el selector de tipo
prepara el efecto en el hilo de control antes de encolarlo. Los controles de
acorde envían juntas todas las notas y las liberan al soltar el botón.

La línea `Chain:` imprime los seis slots (`Rvb`, `Dly`, `Cho`, `Drv`, `Flt`),
un `*` si el slot está activo y `-` si está bypass, y la latencia total de
insertos + limitador publicada por el procesador.

## Cambio de tipo en caliente

El hilo de control construye y prepara el efecto nuevo (asignaciones incluidas),
envía un `SwapEffect` con la dirección del objeto por la cola SPSC y transfiere
la propiedad. El hilo de audio hace un crossfade de 20 ms, devuelve el efecto
viejo por una cola de retiro y el control lo destruye. Nunca se ejecuta
`delete` en el callback.

La latencia de cada slot se reserva al máximo del rack en `prepare()`: si el
rack contiene el drive sobremuestreado, todos los slots reservan 32 muestras
para que cambiar filtro↔drive no mueva el camino seco a mitad del crossfade. La
cadena por defecto cuesta 6 × 32 = 192 muestras (4 ms); el limitador añade 48
muestras (1 ms). Un rack sin insertos con latencia no reserva nada.

## Colas y bypass

- Al conmutar bypass o mezcla, el wet y el dry se rampan a 10/20 ms.
- Un efecto bypass con cola sigue procesando hasta que la salida cae por debajo
  de −80 dB durante 100 ms, o hasta agotar su presupuesto de cola (30 s para
  reverb, 20 s para delay); luego se resetea y se deja de procesar.
- El camino seco se retarda la latencia del slot para que dry y wet no formen
  peine. Un bypass conserva esa latencia, y el test lo comprueba muestra a
  muestra.
- Los cambios de tipo usan el mismo crossfade; la cola del efecto viejo se corta
  al terminar el fade, no se reproduce entera.

## Medidas de esta entrega

Tests Debug/Release en la máquina de desarrollo:

- Delay a 100 ms: primer eco en la muestra 4801 (objetivo 4800, tolerancia ±1).
- Reverb con decay 1 s: RT60 medido 0.88 s; IR finita y decreciente.
- Drive a 7 kHz: alias en 13 kHz **−94.2 dBc con 2×** frente a **−32.5 dBc sin
  sobremuestreo** (61.7 dB de mejora); compensación RMS 0.0707 → 0.0710.
- Limitador: latencia 48 muestras (1.00 ms), techo −1 dBFS, release 80 ms.
- Filtro: máxima resonancia estable; cola silenciosa sin picos de CPU por
  denormales (FTZ/DAZ activo).
- Callback completo: 32 voces con Pad (unison 4) + 6 insertos + limitador,
  Release, 256 frames: **2.0–2.4 ms/bloque = 38–45 % del presupuesto de
  5.33 ms** en varias ejecuciones, latencia reportada 240 muestras (5 ms).
