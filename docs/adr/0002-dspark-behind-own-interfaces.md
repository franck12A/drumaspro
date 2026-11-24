# ADR 0002 — DSPark detrás de interfaces propias

- **Estado:** aceptado (Etapa 2)
- **Fecha:** 2026-11-23

## Contexto

DSPark 1.7.0 (MIT, header-only, C++20) aporta oscilador band-limited, ADSR,
suavizado de parámetros, cola SPSC y guard de denormales: exactamente las
primitivas que necesita el prototipo. Pero el proyecto va a vivir muchos años y
la librería puede cambiar de API, dejar de mantenerse o resultar inadecuada para
algún procesador.

## Decisión

Ningún tipo de DSPark aparece en la API pública de `core`, `fx` ni `synth`. Cada
procesador queda detrás de una interfaz propia:

- `core::SpscQueue<T, N>` envuelve `dspark::SpscQueue`.
- `core::SmoothedValue` envuelve `dspark::SmoothedValue`.
- `core::DenormalGuard` envuelve `dspark::DenormalGuard`.
- `synth::IOscillator` / `synth::IEnvelope` son interfaces propias;
  `SineOscillator` y `AdsrEnvelope` son las implementaciones sobre DSPark.
  (Etapa 4: `SineOscillator` se sustituyó por `BandLimitedOscillator`, que usa
  la tabla minBLEP compartida de DSPark; ver [ADR 0004](0004-band-limited-oscillators.md).)

También se fija la versión: el árbol vendorizado es un subconjunto del tag
`v1.7.0` (commit `e5e13a2`), no de una rama.

## Motivos

- Sustituir DSPark por código propio (o por otra librería) toca un puñado de
  ficheros, no la aplicación.
- Las interfaces propias son más pequeñas que las de la librería y obligan a
  declarar el contrato de hilos en cada tipo, que es donde se necesita.
- Los tests se escriben contra nuestras interfaces, no contra la librería: si
  DSPark cambia de comportamiento, el test lo detecta igual.

## Consecuencias

- Una capa de indirección, en la práctica cero coste: son `inline` y los objetos
  son miembros por valor, no punteros polimórficos, en el camino de audio.
- `engine` no necesita DSPark en absoluto; solo `core` y `synth` lo incluyen.
- Si un procesador futuro necesita una capacidad que la interfaz no expone, se
  amplía la interfaz propia, no se filtra el tipo de DSPark.
