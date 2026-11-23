# ADR 0001 — miniaudio como capa de E/S de audio

- **Estado:** aceptado (Etapa 2)
- **Fecha:** 2026-11-23

## Contexto

El motor necesita abrir el dispositivo de reproducción por defecto en Windows
10/11 con WASAPI, 48 kHz, estéreo, formato float y un tamaño de buffer
configurable. El producto se vende con código cerrado, así que las licencias
deben ser permisivas, y el motor no puede depender de Qt.

Las alternativas razonables eran RtAudio, PortAudio, SDL2 o hablar WASAPI
directamente.

## Decisión

Vendorizar **miniaudio 0.11.26** (tag `0.11.26`) y usarlo como única capa de
dispositivo. Solo el backend WASAPI se usa en la práctica; el resto de backends
siguen compilados pero inactivos, lo que permite probar en otras máquinas sin
cambiar código.

Se compila en un único target estático (`drumas::miniaudio`) con decodificadores,
encoder, resource manager, node graph, engine y generadores desactivados: no
usamos nada de eso y ahorra tiempo de compilación y superficie.

## Motivos

- **Licencia**: dominio público o MIT-0, a elección; compatible con distribución
  comercial cerrada sin obligaciones de atribución.
- **Un solo fichero, sin dependencias**: vendorizar es copiar dos ficheros, sin
  submodules ni build systems ajenos.
- **WASAPI de primera clase**: en Windows elige WASAPI por prioridad y expone el
  nombre del backend en tiempo de ejecución (`ma_get_backend_name`), que la app
  imprime para que no haya dudas de qué se abrió.
- **API C estable y pequeña**: el callback tiene la forma que necesitamos
  (`dataCallback`) y no impone modelo de hilos propio.
- **ASIO**: miniaudio no lo soporta; queda para una fase futura con una capa de
  E/S propia, sin tocar `AudioGraph`.

## Consecuencias

- La latencia real (WASAPI *shared*) no es consultable a través de la API de
  dispositivo de miniaudio; se informa una cota inferior y el periodo observado.
- `AudioEngine` encapsula todo el C de miniaudio: la cabecera pública no incluye
  `miniaudio.h`, así que cambiar de backend no afecta a nadie más.
- El uso de `MA_NO_*` obliga a revisar los defines si más adelante se quiere
  decodificar audio (por ejemplo, para presets con muestras).
