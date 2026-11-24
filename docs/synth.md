# El sintetizador (Etapa 4; cadena de efectos en la Etapa 5A)

Una sola arquitectura de voz parametrizable. Pad, pluck y teclas son **patches**
(conjuntos de valores) sobre esa voz; añadir un instrumento es añadir una
entrada a [`src/synth/PatchBank.h`](../src/synth/PatchBank.h), no una clase.

## Camino de señal de una voz

```
                    ┌─ osc 1: 1..4 capas minBLEP ─┐
note-on ─ patch ──► │   detune + paneo             │
                    ├─ osc 2: 1..4 capas minBLEP ─┤
                    ├─ sub: seno a -1 octava ─────┤──► SVF 2.º orden ──► amp
                    └─ ruido blanco ──────────────┘      estéreo        │
                                                                         ▼
                                     ADSR amp × velocity × 0.20 ──► mezcla L/R
                                     ADSR filtro + LFO ──► cutoff
```

- **Osciladores** ([ADR 0004](adr/0004-band-limited-oscillators.md)):
  seno, triángulo, sierra y pulso con ancho, corregidos con la tabla minBLEP
  compartida de DSPark. El pulso es la diferencia de dos sierras limitadas en
  banda, sin continua. Medido entre −86 y −105 dBc de alias audible entre 1 y
  3 kHz, frente a −31/−15 dBc de PolyBLEP/ingenua.
- **Unison**: hasta 4 capas por oscilador, con detune y paneo constante por
  capa. Cada capa lleva su propia fase inicial para que no sumen en fase al
  atacar.
- **Filtro** ([ADR 0006](adr/0006-filter.md)): `StateVariableFilter` de DSPark
  en modo pasa-bajos, una instancia por voz que comparte coeficientes entre L y
  R. Cutoff modulado por la envolvente de filtro, la velocity y el LFO.
- **Envolventes**: dos ADSR por voz (amplitud y filtro), con retrigger legato
  (la segunda nota continúa desde el nivel actual, sin salto).
- **Velocity**: escala el volumen (`velocity.volume`) y abre el filtro hasta
  2 octavas (`velocity.brightness`).
- **LFO global** ([ADR 0007](adr/0007-patch-switching-and-live-parameters.md)):
  seno, triángulo, sierra o cuadrado, destino cutoff y vibrato. Un solo LFO
  por muestra para todo el pool: coste determinista y fase coherente en el
  acorde.
- **Etapa maestra:** desde la Etapa 5A, el bus de voces pasa por los seis slots
  de efectos ([`effects.md`](effects.md)), la ganancia suavizada y el limitador
  look-ahead ([ADR 0011](adr/0011-master-limiter.md)). El soft-clip antiguo
  ([ADR 0005](adr/0005-master-soft-clip.md)) queda como safety clip de último
  recurso, no como protección principal.

## Patches de fábrica

| Patch | Carácter | Unison | Amplitud (A/D/S/R) | Filtro | Notas |
|---|---|---|---|---|---|
| Pad | cálido, lento, ancho | 4 capas, 14 cents, estéreo 0.8 | 700 / 1500 / 0.85 / 2600 ms | 750 Hz, res 0.18, env 2.2 oct | LFO 0.22 Hz, vibrato 4 cents |
| Pluck | ataque inmediato, cola corta | 2 capas, 8 cents | 0.8 / 260 / 0.03 / 220 ms | 480 Hz, res 0.42, env 4 oct | la envolvente de filtro hace el "pluck" |
| Keys | piano eléctrico/órgano ligero | 2 capas, 3 cents | 2.5 / 1200 / 0.35 / 800 ms | 3200 Hz, res 0.12, env 1.6 oct | triángulo + seno, brillo muy sensible a velocity |

La calibración de nivel está pensada para que un acorde de 4 notas no llegue
al limitador. En las pruebas offline, el pico de un acorde de cuatro notas con
Pad es ~0.30, con Pluck ~0.25 y con Keys ~0.13 (velocidad 0.8).

## Cambio de patch y parámetros en vivo

- `SelectPatch` cambia el patch de las **notas siguientes**. Las voces que ya
  suenan conservan el patch que capturaron y terminan su release con él; no
  hay ningún parámetro que cambie a mitad de nota, así que no hay clic.
- `SetParam` con los ids en vivo (cutoff, resonancia, tiempos ADSR, master)
  actualiza un overlay `LiveParameters` que se aplica a **todas** las voces con
  rampa de 10 ms. Una voz nueva arranca en el valor en vivo.
- Los parámetros de timbre que no están en el overlay (forma de onda, niveles,
  unison, profundidad de LFO) solo cambian con un patch nuevo o una nota nueva.

## Rendimiento

Medido en la máquina de desarrollo, Release, bloque de 256 frames a 48 kHz,
32 voces sonando con Pad (2 osciladores × 4 capas + sub + filtro + 2 ADSR +
LFO): **1,55-1,93 ms por bloque = 29-36 % del presupuesto de 5,33 ms**. Con los
seis insertos y el limitador de la Etapa 5A el mismo benchmark mide 2,1–2,4 ms
(40–45 %); ver [`effects.md`](effects.md). El benchmark vive en
`tests/synth/test_performance.cpp` e imprime el dato; no es un umbral
dependiente de la máquina.

## Contratos de hilos

Todo lo que toca el callback es RT-SAFE tras `prepare()`: sin asignaciones, sin
locks, sin E/S. `prepare()` (setup) asigna los buffers de mezcla, construye la
tabla minBLEP compartida y registra los parámetros. El comando `SelectPatch`
sigue siendo trivialmente copiable y viaja por la cola SPSC. Ver
[`rt-safety.md`](rt-safety.md).
