# drumaspro

Motor de un instrumento musical virtual de escritorio para Windows 10/11
(acordes, sintetizadores, efectos en tiempo real, atmósferas y ritmos) con
estética de hardware retrofuturista. **No es un DAW.**

Este repositorio contiene, por ahora, **la Etapa 2: el prototipo del motor de
audio**. Es una aplicación de consola que reproduce notas senoidales limpias por
WASAPI con la arquitectura definitiva desde el primer día: módulos CMake
separados, hilo de audio libre de asignaciones, comandos por cola lock-free y
tests automatizados.

Lo que **no** hay todavía (fuera de alcance en esta etapa): UI (Qt/QML),
acordes, tonalidades, efectos, atmósferas, ritmos, presets e instalador.

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
- `Esc` sale e imprime el resumen: backend, dispositivo, frecuencia, bloques,
  glitches de stream, callbacks pasados de tiempo, comandos aplicados/descartados
  y tiempo máximo de callback.

Ejemplo de salida:

```
Backend        : WASAPI
Device         : Altavoces (Realtek(R) Audio)
Sample rate    : 48000 Hz
Channels       : 2
Requested      : 256 frames
Device period  : 480 frames (reported by the driver)
Latency (est.) : 10.00 ms (buffer only)
```

## Tests

```powershell
ctest --preset vs2026-debug
ctest --preset vs2026-release
```

29 casos: cola SPSC (FIFO, lleno/vacío, dos hilos), suavizado de parámetros,
frecuencia del oscilador por cruces por cero, fases del ADSR, pool de voces con
*stealing*, y el grafo de audio (comandos, intercalado, bloques grandes,
pitch de extremo a extremo y **cero asignaciones en el bloque de audio**).

## Estructura

```
src/core      tipos, comandos, cola SPSC, suavizado, interfaz de procesador
src/synth     oscilador seno, ADSR, voz, pool de voces, SynthProcessor
src/engine    AudioGraph (camino en tiempo real) y AudioEngine (miniaudio/WASAPI)
src/app       aplicación de consola
tests/        Catch2 + CTest
third_party/  miniaudio 0.11.26 y DSPark 1.7.0, vendorizados (ver README propio)
docs/         estilo, reglas de tiempo real, ASan y ADRs
```

Grafo de dependencias (sin ciclos):

```
core  (INTERFACE)
 ├── synth  (INTERFACE)
 ├── engine (STATIC)  -> miniaudio
 └── app    (EXECUTABLE) -> synth + engine
```

`core`, `synth`, `fx` y `engine` no incluyen nada de Qt ni de UI; `engine` no
conoce `synth`, solo la interfaz `core::IAudioProcessor`.

## Documentación

- [`docs/rt-safety.md`](docs/rt-safety.md) — reglas del hilo de audio y cómo se
  verifican.
- [`docs/style.md`](docs/style.md) — guía de estilo y convenciones.
- [`docs/asan.md`](docs/asan.md) — AddressSanitizer con MSVC.
- [`docs/adr/`](docs/adr) — decisiones de arquitectura.
- [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) — licencias verificadas
  (todo compatible con distribución comercial de código cerrado).

## Limitaciones conocidas de esta etapa

- La latencia mostrada es la del buffer del driver: es una **cota inferior**; el
  periodo real de callback se imprime aparte.
- La señal se recorta en [-1, 1] en la etapa maestra (todavía no hay limitador).
- El prototipo no tiene UI: teclas en consola, sin control de parámetros salvo
  los comandos internos.
- El workflow de CI y la escucha real del tono durante 10 minutos requieren
  verificación externa (ver la entrega de la etapa).
