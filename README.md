# drumaspro

Motor de un instrumento musical virtual de escritorio para Windows 10/11
(acordes, sintetizadores, efectos en tiempo real, atmósferas y ritmos) con
estética de hardware retrofuturista. **No es un DAW.**

Este repositorio contiene la **Etapa 5A: cadena de efectos**. Sobre el motor de
síntesis sustractiva de la Etapa 4 hay seis slots en serie (reverb, delay,
chorus, drive y dos filtros), bypass y mezcla por slot, cambio de tipo en
caliente y un limitador maestro con look-ahead. Sigue siendo una aplicación de
consola, con la arquitectura definitiva desde el primer día: módulos CMake
separados, hilo de audio libre de asignaciones, comandos por cola lock-free,
parámetros con registro propio y tests automatizados.

Lo que **no** hay todavía (fuera de alcance): UI (Qt/QML), presets, atmósferas,
ritmos, arpegios, joystick, instalador y los efectos de la Etapa 5B (flanger,
phaser, tremolo, vibrato, lo-fi, shimmer, EQ).

## Requisitos

- Windows 10/11 x64.
- **CMake 3.21 o superior** (los presets usan `CMakePresets.json` v6; con
  CMake 4.x se ha verificado el proyecto).
- **MSVC con C++20**: Visual Studio 2022 (17.x) o Visual Studio 2026 (18.x).
  - DSPark 1.7.0 declara soporte probado desde MSVC 19.50 (VS 2026); VS 2022
    compila el proyecto en CI, pero es la combinación que el upstream no
    verifica.
- No hace falta instalar dependencias: `third_party/` está vendorizado y Catch2
  se descarga al configurar (requiere red la primera vez).

## Compilar

```powershell
cmake --preset vs2026            # o: cmake --preset vs2022
cmake --build --preset vs2026-debug
cmake --build --preset vs2026-release
```

Los presets de VS generan un árbol multi-configuración en
`build/<preset>/`, con Debug y Release disponibles en la misma solución.

Si `cmake` no está en el `PATH`, usa el que viene con Visual Studio:
`C:\Program Files\Microsoft Visual Studio\2026\...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
(la ruta exacta depende de la instalación).

## Ejecutar

```powershell
.\build\vs2026\src\app\Debug\drumaspro.exe
.\build\vs2026\src\app\Debug\drumaspro.exe --buffer=128 --rate=48000
```

- `A S D F G H J` tocan C4 D4 E4 F4 G4 A4 B4 (notas de 400 ms por pulsación: una
  consola no envía eventos de "tecla soltada").
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

75 casos: cola SPSC (FIFO, lleno/vacío, dos hilos), suavizado de parámetros,
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
del presupuesto de bloque.

Mediciones reales de esta entrega:

- Alias audible de la sierra minBLEP: −94,8 / −92,2 / −86,1 dBc a 1 / 1,5 /
  3 kHz (sierra ingenua: −15,0 / −13,1 / −10,3 dBc). Pulso: entre −93,7 y
  −104,1 dBc.
- Delay a 100 ms: primer eco en la muestra 4801 (±1). Reverb con decay 1 s:
  RT60 medido 0.88 s. Drive a 7 kHz: −94,2 dBc de alias a 13 kHz con 2× frente a
  −32,5 dBc sin oversampling (61,7 dB de mejora). Limitador: 48 muestras de
  latencia y techo estricto de −1 dBFS.
- CPU del callback con 32 voces, unison 4, **seis insertos y limitador**
  (Release, 256 frames): 2,14 ms por bloque, **40,0 % del presupuesto de
  5,33 ms**, latencia total 240 muestras (5 ms) en la máquina de desarrollo.

## Estructura

```
src/core      tipos, comandos, cola SPSC, suavizado, registro de parámetros,
              guard de denormales
src/fx        IEffect, fábrica, descripción de cadena, seis slots, reverb,
              delay, chorus, drive, filtro y limitador look-ahead
src/synth     oscilador minBLEP, LFO, voz sustractiva, filtro, patches,
              pool de voces, SynthProcessor
src/engine    AudioGraph (camino en tiempo real) y AudioEngine (miniaudio/WASAPI)
src/app       aplicación de consola
tests/        Catch2 + CTest, con contador de asignaciones
third_party/  miniaudio 0.11.26 y DSPark 1.7.0, vendorizados (ver README propio)
docs/         estilo, reglas de tiempo real, parámetros, sintetizador, efectos y ADRs
```

Grafo de dependencias (sin ciclos):

```
core  (INTERFACE)
 ├── fx     (INTERFACE)
 ├── synth  (INTERFACE) -> fx
 ├── engine (STATIC)    -> miniaudio
 └── app    (EXECUTABLE) -> synth + engine
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
- Solo cutoff, resonancia, tiempos ADSR, LFO rate/shape y master son editables
  en vivo; el resto de parámetros de timbre requiere un patch nuevo o una nota
  nueva.
- La latencia mostrada por el motor es la de insertos + limitador; la latencia
  del driver es una **cota inferior** adicional y el periodo real de callback se
  imprime aparte.
- El prototipo no tiene UI gráfica: teclas en consola.
- La calidad de timbre, el carácter de la reverb, el pumping del limitador, la
  ausencia de clics y el "se siente bien" de cada efecto requieren escucha
  humana; ver la checklist de la entrega de la etapa.
- El workflow de CI de GitHub Actions no se ha podido consultar desde el
  entorno de desarrollo; la verificación local (Debug y Release, `/W4 /WX`,
  75/75 tests) sí está hecha.
