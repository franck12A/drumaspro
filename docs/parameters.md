# Parámetros del sintetizador

Todos los parámetros viven en `core::ParameterRegistry`
([`src/core/ParameterRegistry.h`](../src/core/ParameterRegistry.h)): un id
estable de 32 bits, un nombre único, un rango real, un valor por defecto, una
unidad y una curva (lineal o logarítmica). El registro es genérico y no
depende de `synth`; los efectos y la UI de la Etapa 5/8 lo reutilizarán tal
cual.

## Convenciones

- **ID**: FNV-1a 32 bits del nombre, calculado en compilación con
  `core::makeParameterId`. Nunca cambia una vez publicado: viaja en `SetParam`
  y viajará en los presets. Renombrar un parámetro cambia su id, así que se
  trata como un cambio incompatible.
- **Rango**: valores reales. Las conversiones desde/hacia 0..1 recortan al
  rango; un valor fuera de rango nunca se rechaza en silencio.
- **Curva logarítmica**: pasos normalizados iguales son ratios iguales. Se usa
  en frecuencias y tiempos, que el oído juzga en proporción.
- **En vivo**: el parámetro se puede mover con `SetParam` sobre voces ya
  sonando (ver [ADR 0007](adr/0007-patch-switching-and-live-parameters.md)).
  El resto son parámetros de patch: cambian al seleccionar otro patch o al
  tocar una nota nueva.

## Tabla

| ID (hex) | Nombre | Rango | Unidad | Curva | Defecto | En vivo |
|---|---|---|---|---|---|---|
| `0xE0A7DF5C` | `osc1.waveform` | 0..3 | — | lineal | 1 (triángulo) | no |
| `0x89CED89B` | `osc1.level` | 0..1 | proporción | lineal | 0.5 | no |
| `0x59C9F136` | `osc1.detune` | −50..50 | cents | lineal | 0 | no |
| `0xE229FE60` | `osc1.pulseWidth` | 0.05..0.95 | proporción | lineal | 0.5 | no |
| `0xD5DBA827` | `osc2.waveform` | 0..3 | — | lineal | 1 (triángulo) | no |
| `0x96BB6272` | `osc2.level` | 0..1 | proporción | lineal | 0.5 | no |
| `0xB21057F1` | `osc2.detune` | −50..50 | cents | lineal | 0 | no |
| `0x6AAEA6D7` | `osc2.pulseWidth` | 0.05..0.95 | proporción | lineal | 0.5 | no |
| `0x12008F49` | `sub.level` | 0..1 | proporción | lineal | 0 | no |
| `0x56AB25F5` | `noise.level` | 0..1 | proporción | lineal | 0 | no |
| `0xB527EE64` | `unison.voices` | 1..4 | capas | lineal | 1 | no |
| `0xE9689666` | `unison.detune` | 0..50 | cents | lineal | 0 | no |
| `0xE3734027` | `unison.stereo` | 0..1 | proporción | lineal | 0 | no |
| `0xBF58F22E` | `filter.cutoff` | 20..20000 | Hz | logarítmica | 5000 | **sí** |
| `0x57213419` | `filter.resonance` | 0..1 | proporción | lineal | 0.1 | **sí** |
| `0x9FCCDCF3` | `filter.envOctaves` | 0..8 | octavas | lineal | 2 | no |
| `0x4CED43C3` | `filter.attack` | 0.5..10000 | ms | logarítmica | 5 | **sí** |
| `0x05E0A1BB` | `filter.decay` | 0.5..10000 | ms | logarítmica | 200 | **sí** |
| `0xD6FFEC18` | `filter.sustain` | 0..1 | proporción | lineal | 0.4 | **sí** |
| `0xD6D1D628` | `filter.release` | 0.5..20000 | ms | logarítmica | 200 | **sí** |
| `0x282C110D` | `amp.attack` | 0.5..10000 | ms | logarítmica | 5 | **sí** |
| `0xEAE2F8ED` | `amp.decay` | 0.5..10000 | ms | logarítmica | 120 | **sí** |
| `0xA1553E7A` | `amp.sustain` | 0..1 | proporción | lineal | 0.7 | **sí** |
| `0xC293CC2E` | `amp.release` | 0.5..20000 | ms | logarítmica | 180 | **sí** |
| `0x41554E66` | `velocity.volume` | 0..1 | proporción | lineal | 0.7 | no |
| `0xF4CFFA1B` | `velocity.brightness` | 0..1 | proporción | lineal | 0 | no |
| `0x8576DCEF` | `lfo.shape` | 0..3 | — | lineal | 0 (seno) | **sí** |
| `0xBEB4B720` | `lfo.rate` | 0.05..20 | Hz | logarítmica | 5 | **sí** |
| `0x1EEB9558` | `lfo.cutoffDepth` | 0..1 | proporción | lineal | 0 | no |
| `0x596DC8F5` | `lfo.pitchDepth` | 0..100 | cents | lineal | 0 | no |
| `0xC569D500` | `master.gain` | 0..1 | proporción | lineal | 0.7 | **sí** |

### Semántica de las modulaciones

- `filter.envOctaves`: la envolvente de filtro (0..1) desplaza el cutoff hasta
  ese número de octavas por encima de su valor base.
- `velocity.brightness`: con velocity 1, suma hasta 2 octavas de brillo
  (proporcional a 0..1).
- `lfo.cutoffDepth`: profundidad 1 equivale a ±2 octavas de cutoff.
- `lfo.pitchDepth`: vibrato en cents de pico.
- `osc*.detune`: desafinación fija del oscilador; `unison.detune` es la
  apertura total entre capas. Se suman.
- `unison.voices` capas por oscilador, repartidas en paneo constante
  (`unison.stereo`) y normalizadas por 1/n para no cambiar el nivel al subir
  las capas.

## Fuera del registro en esta etapa

Los índices de patch (`SelectPatch`) no son parámetros de sonido: los gestiona
el processor como una selección, no como un valor continuo. La serialización
de patches a JSON llegará en la Etapa 8 y escribirá una entrada por cada campo
de `Patch` usando estos nombres.
