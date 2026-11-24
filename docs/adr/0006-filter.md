# ADR 0006 — Filtro pasa-bajos resonante

- **Estado:** aceptado (Etapa 4)
- **Fecha:** 2026-11-24

## Contexto

Cada voz necesita un pasa-bajos resonante cuyo cutoff se modula **por muestra**
con la envolvente de filtro, la velocity y el LFO. Debe ser estable a
resonancia alta y en los extremos de cutoff, y no debe producir chasquidos al
cambiar de parámetro. DSPark ofrece dos candidatos.

### `dspark::StateVariableFilter` (TPT, 2.º orden)

- Topología Zavalishin/Simper de 2.º orden (12 dB/oct), pensada explícitamente
  para modulación por muestra: `setCutoff()` recalcula coeficientes sin
  artefactos y sin división en el camino caliente.
- Estable a cualquier cutoff; el cutoff se limita a [20 Hz, 0,499·fs] y la
  resonancia 0..1 se mapea a Q 0,5..50 (cerca, pero no en autooscilación).
- Rechaza NaN/Inf en los setters, así que un parámetro corrupto no envenena el
  filtro para siempre.
- Contrato de hilos: los setters pertenecen al hilo de audio; es justo el uso
  que hace una voz.

### `dspark::LadderFilter` (Moog, 4 polos)

- Excelente sonido y 24 dB/oct, pero sus setters publican coeficientes con
  atómicos pensados para **un productor de control** y `processBlock()` toma una
  instantánea por bloque: no es la herramienta para modular cutoff cada muestra
  desde la envolvente.
- La ganancia de paso cae con la resonancia (≈ −14 dB a resonancia 1) y en
  resonancia 1 el bucle es marginal sin drive; habría que compensar ganancia y
  limitar la resonancia para que los patches no se autooscilaran.

## Decisión

`synth::ResonantLowPassFilter` envuelve **`StateVariableFilter`** en modo
LowPass, con **una instancia por voz** que procesa L y R: los coeficientes se
comparten y cada canal mantiene su propio estado integrador, así el cutoff se
calcula una vez por muestra y no dos.

La envoltura mantiene DSPark fuera de la API pública, ignora valores no finitos
y documenta el contrato de hilos. No se añade sobremuestreo: el SVF es estable y
el alias del filtro no es el problema en esta etapa.

## Consecuencias

- `setCutoff()` calcula una tangente por llamada. A 32 voces × 48 kHz son ~1,5 M
  tangentes por segundo; el benchmark de la etapa mide el coste real. Si
  resultara excesivo, la optimización natural es cachear el coeficiente por
  bandas de 0,1 % o usar `fastTan`, sin cambiar la interfaz.
- No hay autooscilación: Q máximo 50. Si un patch futuro la necesita, se añade
  un lazo de realimentación propio o se sube el rango de resonancia, con test
  de estabilidad.
- El filtro es de 2.º orden; el ladder de 24 dB/oct queda como alternativa para
  un timbre más agresivo en una etapa posterior, detrás de la misma interfaz.
