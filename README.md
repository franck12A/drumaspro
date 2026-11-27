# drumaspro

Motor de un instrumento musical virtual de escritorio para Windows 10/11
(acordes, sintetizadores, efectos en tiempo real, atmósferas y ritmos) con
estética de hardware retrofuturista. **No es un DAW.**

Este repositorio contiene el motor de síntesis sustractiva, la cadena de seis
efectos de la Etapa 5A y una interfaz de escritorio Qt/QML opcional. La interfaz
ofrece controles de sintetizador, acordes diatónicos, patches y cadena de
efectos; la aplicación de consola sigue disponible para diagnóstico y
automatización. Ambos usan el mismo motor y cola de comandos.

Lo que **no** hay todavía (fuera de alcance): presets, atmósferas,
ritmos, arpegios, joystick, instalador y los efectos de la Etapa 5B (flanger,
phaser, tremolo, vibrato, lo-fi, shimmer, EQ).

## Requisitos

- Windows 10/11 x64.
- **CMake 3.21 o superior** (los presets usan el esquema 3 de
  `CMakePresets.json`; con CMake 4.x se ha verificado el proyecto).
- **MSVC con C++20**: Visual Studio 2022 (17.x) o Visual Studio 2026 (18.x).
  - DSPark 1.7.0 declara soporte probado desde MSVC 19.50 (VS 2026); VS 2022
    compila el proyecto en CI, pero es la combinación que el upstream no
    verifica.
- No hace falta instalar dependencias: `third_party/` está vendorizado y Catch2
  se descarga al configurar (requiere red la primera vez).
- La UI es opcional y requiere Qt 6.8.3 para MSVC x64. En desarrollo local el
  preset busca el SDK bajo `.tools/Qt/6.8.3/msvc2022_64`; sin Qt se compilan
  igualmente el motor, la consola y las pruebas.

## Compilar

```powershell
cmake --preset vs2026            # o: cmake --preset vs2022
cmake --build --preset vs2026-debug
cmake --build --preset vs2026-release
```

Para incluir la UI:

```powershell
cmake --preset vs2026 -DDRUMAS_BUILD_UI=ON
cmake --build --preset vs2026-release --target drumaspro_qt
```

Para configurar, compilar en Release, desplegar las DLL de Qt junto al
ejecutable y abrir la ventana:

```powershell
.\tools\run-ui.ps1
```

Los presets de VS generan un árbol multi-configuración en
`build/<preset>/`, con Debug y Release disponibles en la misma solución.

Si `cmake` no está en el `PATH`, usa el que viene con Visual Studio:
`C:\Program Files\Microsoft Visual Studio\2026\...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
(la ruta exacta depende de la instalación).

## Ejecutar

```powershell
.\build\vs2026\src\ui\qt\Debug\drumaspro.exe
.\build\vs2026\src\app\Debug\drumaspro_console.exe
.\build\vs2026\src\app\Debug\drumaspro_console.exe --buffer=128 --rate=48000
```

La UI permite elegir patch, tonalidad mayor/menor, tocar acordes manteniendo el
botón pulsado, tocar el piano en pantalla o desde el teclado del ordenador,
controlar los parámetros del sintetizador y editar los seis slots de efectos. Al
seleccionar un slot, la franja inferior muestra sus parámetros reales (registro,
rango, unidad y curva) para moverlos en vivo.

El botón `SETUP` abre los ajustes persistentes (`%APPDATA%\Drumaspro\settings.json`,
JSON con `schemaVersion`, escritura atómica y carga tolerante a archivos
corruptos): piel (default/ámbar/noche), 6 presets de acento, escala tipográfica
(90/100/110/125 %), sensibilidad de perillas, curva de velocity, octava por
defecto, séptimas diatónicas, último tono/escala/patch y el mapeo de los pads.
Cada pad puede ser diatónico automático o manual (raíz, calidad, inversión y
octava) con clic derecho o desde `SETUP > CHORD PADS > EDIT`. `LATCH` mantiene el
acorde sonando hasta pulsar otro o apagarlo, con LED.

- `A W S E D F T G Y H U J K` tocan una octava cromática desde C (C C# D D#
  E F F# G G# A A# B C); `Z` / `X` bajan y suben la octava y el LCD muestra la
  octava actual. Sin auto-repeat y sin interceptar teclas de controles
  enfocados.
- El botón `KEYS` cambia la franja de acordes por el piano en pantalla: dos
  octavas desde C3 a 960×600 y tres desde 1100 px de ancho. Clic y arrastre
  hacen glissando y la altura del clic ajusta la velocity.
- En la consola, `A S D F G H J` tocan C4 D4 E4 F4 G4 A4 B4 (notas de 400 ms por
  pulsación: una consola no envía eventos de "tecla soltada").
- `F1` / `F2` / `F3` eligen Pad, Pluck o Keys.
- `1` / `2` / `3` eligen el parámetro del sintetizador: cutoff, resonancia o
  release.
- `Q W E R T Y` seleccionan los slots de efectos 1..6; `B` conmuta bypass, `N`
  cicla el tipo y `Tab` alterna foco sintetizador/efectos.
- Con foco en efectos, `1` / `2` / `3` eligen parámetro y `4` la mezcla dry/wet;
  `0` vuelve a los parámetros del sintetizador.
- `↑` / `↓` (o `+` / `-`) suben y bajan el parámetro seleccionado.
- `Esc` sale e imprime el resumen: backend, dispositivo, frecuencia, bloques,
  glitches de stream, callbacks pasados de tiempo, comandos aplicados/descartados,
  latencia del instrumento y tiempo máximo de callback.

La línea de estado muestra el patch activo, cutoff, resonancia, release, voces
activas, latencia de insertos + limitador, bloques, xruns y tiempo máximo de
callback. `Chain:` imprime el estado compacto de los seis slots.

## Tests

```powershell
ctest --preset vs2026-debug
ctest --preset vs2026-release
```

La suite cubre la cola SPSC (FIFO, lleno/vacío, dos hilos y batches todo-o-nada),
suavizado de parámetros,
registro de parámetros (rangos, curvas log, valores fuera de rango), afinación y
alias del oscilador con FFT (y comparación con una sierra ingenua), respuesta y
estabilidad del filtro, fases del ADSR, pool de voces con *stealing*, patches de
fábrica con acordes de cuatro notas, cambio de patch y parámetros en caliente,
la cadena de efectos (orden, bypass con latencia, crossfade de tipo, swap sin
asignaciones), reverb (RT60, IR finita, extremos), delay (±1 muestra, ping-pong,
feedback máximo), chorus, drive (compensación y alias con/sin 2×), filtro
multimodo, limitador (techo estricto y latencia), cola silenciosa sin
denormales, el grafo de audio (**cero asignaciones con 32 voces, 6 insertos y
limitador**, incluido un swap) y un benchmark de CPU que imprime el porcentaje
del presupuesto de bloque; además, teoría musical (calidades, inversiones,
límites MIDI, escalas y acordes diatónicos), FFT de fundamentales, continuidad
de cambio de acorde, niveles de oscilador en vivo, espejo de parámetros/cadena,
modelo de parámetros de efecto, mapeo del teclado del ordenador a MIDI, cambio
de octava, límites 0–127, referencias compartidas entre acorde y piano, ajustes
(ida y vuelta, archivo corrupto/vacío/truncado, campos desconocidos, versión
futura, rangos clampeados), pads manuales frente a diatónicos, séptimas, latch,
aislamiento de Qt, guardia del singleton QML y pruebas de modelos, carga QML y
captura offscreen. La CI deja la UI Qt
desactivada; para ejecutar su prueba local hay que configurar con
`DRUMAS_BUILD_UI=ON`.

Mediciones reales de esta entrega:

- Alias audible de la sierra minBLEP: −94,8 / −92,2 / −86,1 dBc a 1 / 1,5 /
  3 kHz (sierra ingenua: −15,0 / −13,1 / −10,3 dBc). Pulso: entre −93,7 y
  −104,1 dBc.
- Delay a 100 ms: primer eco en la muestra 4801 (±1). Reverb con decay 1 s:
  RT60 medido 0.88 s. Drive a 7 kHz: −94,2 dBc de alias a 13 kHz con 2× frente a
  −32,5 dBc sin oversampling (61,7 dB de mejora). Limitador: 48 muestras de
  latencia y techo estricto de −1 dBFS.
- CPU del callback con 32 voces, unison 4, **seis insertos y limitador**
  (Release, 256 frames): 2,0–2,4 ms por bloque, **38–45 % del presupuesto de
  5,33 ms**, latencia total 240 muestras (5 ms) en la máquina de desarrollo
  (varias ejecuciones).

## Estructura

```
src/core      tipos, comandos, cola SPSC, suavizado, registro de parámetros,
              guard de denormales
src/music     pitch classes, acordes, inversiones, escalas y grados diatónicos
src/fx        IEffect, fábrica, descripción de cadena, seis slots, reverb,
              delay, chorus, drive, filtro y limitador look-ahead
src/synth     oscilador minBLEP, LFO, voz sustractiva, filtro, patches,
              pool de voces, SynthProcessor
src/engine    AudioGraph (camino en tiempo real) y AudioEngine (miniaudio/WASAPI)
src/bridge    expansión atómica de acordes y espejos de control sin Qt
src/app       aplicación de consola de diagnóstico
src/ui/qt     puente QObject, modelos Qt y UI QML opcional
tests/        Catch2 + CTest, con contador de asignaciones
third_party/  miniaudio 0.11.26 y DSPark 1.7.0, vendorizados (ver README propio)
docs/         estilo, reglas de tiempo real, parámetros, sintetizador, efectos y ADRs
```

Grafo de dependencias (sin ciclos):

```
core  (INTERFACE)
 ├── fx     (INTERFACE)
 ├── synth  (INTERFACE) -> fx
 ├── music  (INTERFACE) -> funciones puras
 ├── engine (STATIC)    -> miniaudio
 ├── bridge (INTERFACE) -> engine + music
 ├── app    (EXECUTABLE) -> synth + engine
 └── ui/qt  (EXECUTABLE, opcional) -> bridge + Qt
```

`core`, `synth`, `fx` y `engine` no incluyen nada de Qt ni de UI; `engine` no
conoce `synth`, solo la interfaz `core::IAudioProcessor`. Añadir un instrumento
es añadir una entrada a `src/synth/PatchBank.h`: no se toca el motor.

## Documentación

- [`docs/synth.md`](docs/synth.md) — arquitectura de la voz, patches, LFO y
  cambio en caliente.
- [`docs/effects.md`](docs/effects.md) — cadena de seis slots, tabla de efectos
  (ID, nombre, rango, unidad, curva), colas, latencia y consola.
- [`docs/parameters.md`](docs/parameters.md) — tabla de parámetros del
  sintetizador.
- [`docs/listening-checklist.md`](docs/listening-checklist.md) — qué escuchar en
  cada patch (pendiente de verificación humana).
- [`docs/ui.md`](docs/ui.md) — interfaz Qt/QML, compilación y modelo de
  comunicación con el motor.
- [`docs/ui-checklist.md`](docs/ui-checklist.md) — comprobación visual y de
  escucha en dispositivo real (pendiente de verificación humana).
- [`docs/screenshots/drumaspro-default-1280x720.png`](docs/screenshots/drumaspro-default-1280x720.png) —
  captura del tema gris-beige.
- [`docs/screenshots/drumaspro-default-960x600.png`](docs/screenshots/drumaspro-default-960x600.png) —
  captura compacta del tema gris-beige.
- [`docs/screenshots/drumaspro-amber-1280x720.png`](docs/screenshots/drumaspro-amber-1280x720.png) —
  captura del tema ámbar.
- [`docs/screenshots/drumaspro-amber-960x600.png`](docs/screenshots/drumaspro-amber-960x600.png) —
  captura compacta del tema ámbar.
- [`docs/screenshots/drumaspro-keys-1280x720.png`](docs/screenshots/drumaspro-keys-1280x720.png) —
  vista del piano en pantalla (tres octavas).
- [`docs/screenshots/drumaspro-keys-960x600.png`](docs/screenshots/drumaspro-keys-960x600.png) —
  vista del piano en la ventana compacta (dos octavas).
- [`docs/screenshots/drumaspro-night-1280x720.png`](docs/screenshots/drumaspro-night-1280x720.png) —
  tema noche con acento fósforo.
- [`docs/screenshots/drumaspro-setup-1280x720.png`](docs/screenshots/drumaspro-setup-1280x720.png) —
  diálogo `SETUP` con piel, acento, escala y mapeo de pads.
- [`docs/rt-safety.md`](docs/rt-safety.md) — reglas del hilo de audio y cómo se
  verifican.
- [`docs/style.md`](docs/style.md) — guía de estilo y convenciones.
- [`docs/asan.md`](docs/asan.md) — AddressSanitizer con MSVC.
- [`docs/adr/`](docs/adr) — decisiones de arquitectura.
- [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) — licencias verificadas
  (todo compatible con distribución comercial de código cerrado).

## Limitaciones conocidas de esta etapa

- La cadena reserva 32 muestras de latencia por slot cuando el rack contiene el
  drive sobremuestreado, para que el cambio de tipo no mueva el camino seco. El
  coste por defecto es 4 ms de insertos + 1 ms de limitador.
- El limitador desactiva true-peak por coste: garantiza techo de muestra, no
  inter-sample peak. El safety clip posterior queda por debajo del techo del
  limitador.
- La sincronía de delay por BPM está reservada en `ChainDescription::tempoBpm`,
  pero todavía no se expone en la consola; el tiempo se edita en ms.
- El LFO es global (una fase para todo el pool): no hay drift analógico por voz.
- Solo cutoff, resonancia, niveles de oscilador, tiempos ADSR, LFO rate/shape y
  master son editables en vivo; el resto de parámetros de timbre requiere un
  patch nuevo o una nota nueva.
- La latencia mostrada por el motor es la de insertos + limitador; la latencia
  del driver es una **cota inferior** adicional y el periodo real de callback se
  imprime aparte.
- el audio requiere escucha humana para validar gusto/timbre; ver la checklist.
- La calidad de timbre, el carácter de la reverb, el pumping del limitador, la
  ausencia de clics y el "se siente bien" de cada efecto requieren escucha
  humana; ver la checklist de la entrega de la etapa.
- La captura QML offscreen se genera en tests y muestra los modelos; no sustituye
  la comprobación humana en una ventana nativa ni la escucha con un dispositivo.
- La verificación local Release (`/W4 /WX`, 99/99 tests con Qt activada) está
  hecha. El workflow de GitHub Actions configura `DRUMAS_BUILD_UI=OFF` para que
  Qt siga siendo opcional, así que no cubre la UI ni sus capturas.
