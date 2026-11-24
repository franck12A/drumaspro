# drumaspro

Motor de un instrumento musical virtual de escritorio para Windows 10/11
(acordes, sintetizadores, efectos en tiempo real, atmósferas y ritmos) con
estética de hardware retrofuturista. **No es un DAW.**

Este repositorio contiene la **Etapa 4: motor de síntesis sustractiva**. Es una
aplicación de consola que reproduce acordes con tres instrumentos de fábrica
(pad, pluck y teclas), con la arquitectura definitiva desde el primer día:
módulos CMake separados, hilo de audio libre de asignaciones, comandos por cola
lock-free, parámetros con registro propio y tests automatizados.

Lo que **no** hay todavía (fuera de alcance en esta etapa): UI (Qt/QML),
efectos, atmósferas, ritmos, presets, arpegios, joystick e instalador.

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
- `1` / `2` / `3` eligen el parámetro a editar: cutoff, resonancia o release.
- `↑` / `↓` (o `+` / `-`) suben y bajan el parámetro seleccionado.
- `Esc` sale e imprime el resumen: backend, dispositivo, frecuencia, bloques,
  glitches de stream, callbacks pasados de tiempo, comandos aplicados/descartados
  y tiempo máximo de callback.

La línea de estado muestra el patch activo, cutoff, resonancia, release, voces
activas, bloques, xruns y el tiempo máximo de callback.

## Tests

```powershell
ctest --preset vs2026-debug
ctest --preset vs2026-release
```

56 casos: cola SPSC (FIFO, lleno/vacío, dos hilos), suavizado de parámetros,
registro de parámetros (rangos, curvas log, valores fuera de rango), afinación y
alias del oscilador con FFT (y comparación con una sierra ingenua), respuesta y
estabilidad del filtro, fases del ADSR, pool de voces con *stealing*, patches de
fábrica con acordes de cuatro notas, cambio de patch y parámetros en caliente,
el grafo de audio (comandos, intercalado, bloques grandes y **cero asignaciones
en el bloque de audio con 32 voces y unison máximo**) y un benchmark de CPU que
imprime el porcentaje del presupuesto de bloque.

Mediciones reales de esta entrega:

- Alias audible de la sierra minBLEP: −94,8 / −92,2 / −86,1 dBc a 1 / 1,5 /
  3 kHz (sierra ingenua: −15,0 / −13,1 / −10,3 dBc). Pulso: entre −93,7 y
  −104,1 dBc.
- CPU del callback con 32 voces y unison 4 (Release, 256 frames): 1,55-1,93 ms
  por bloque, **29-36 % del presupuesto de 5,33 ms** en la máquina de
  desarrollo.

## Estructura

```
src/core      tipos, comandos, cola SPSC, suavizado, registro de parámetros
src/synth     oscilador minBLEP, LFO, voz sustractiva, filtro, patches,
              pool de voces, etapa maestra, SynthProcessor
src/engine    AudioGraph (camino en tiempo real) y AudioEngine (miniaudio/WASAPI)
src/app       aplicación de consola
tests/        Catch2 + CTest, con contador de asignaciones
third_party/  miniaudio 0.11.26 y DSPark 1.7.0, vendorizados (ver README propio)
docs/         estilo, reglas de tiempo real, parámetros, sintetizador y ADRs
```

Grafo de dependencias (sin ciclos):

```
core  (INTERFACE)
 ├── synth  (INTERFACE)
 ├── engine (STATIC)  -> miniaudio
 └── app    (EXECUTABLE) -> synth + engine
```

`core`, `synth`, `fx` y `engine` no incluyen nada de Qt ni de UI; `engine` no
conoce `synth`, solo la interfaz `core::IAudioProcessor`. Añadir un instrumento
es añadir una entrada a `src/synth/PatchBank.h`: no se toca el motor.

## Documentación

- [`docs/synth.md`](docs/synth.md) — arquitectura de la voz, patches, LFO y
  cambio en caliente.
- [`docs/parameters.md`](docs/parameters.md) — tabla de parámetros (ID, nombre,
  rango, unidad, curva).
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

- La etapa maestra usa un **soft-clip** con techo garantizado, no un limitador
  con look-ahead: no hay latencia añadida, pero los picos fuertes se comprimen
  sin control de release. El limitador real puede llegar con los efectos.
- El LFO es global (una fase para todo el pool): no hay drift analógico por voz.
- Solo cutoff, resonancia, tiempos ADSR, LFO rate/shape y master son editables
  en vivo; el resto de parámetros de timbre requiere un patch nuevo o una nota
  nueva.
- La latencia mostrada es la del buffer del driver: es una **cota inferior**; el
  periodo real de callback se imprime aparte.
- El prototipo no tiene UI gráfica: teclas en consola.
- La calidad de timbre, la ausencia de clics y el "se siente bien" de cada patch
  requieren escucha humana; ver la checklist de la entrega de la etapa.
- El workflow de CI de GitHub Actions no se ha podido consultar desde el
  entorno de desarrollo; la verificación local (Debug y Release, `/W4 /WX`,
  56/56 tests) sí está hecha.
