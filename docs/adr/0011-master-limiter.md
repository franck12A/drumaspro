# ADR 0011 — Limitador look-ahead y retirada del soft-clip como protección principal

- **Estado:** aceptado (Etapa 5A); sustituye el papel de protección de ADR 0005
- **Fecha:** 2026-11-24

## Contexto

La Etapa 4 protegía la salida con un soft-clip estático de codo 0.7. Funciona,
pero comprime los picos sin control de release y colorea cualquier material que
supere 0.7. La Etapa 5 introduce la cadena de efectos y necesita un techo
estricto, latencia declarada y una etapa medible.

## Decisión

`fx::MasterLimiter` envuelve `dspark::Limiter`:

- techo **−1 dBFS** (0.891 lineal), estricto a la salida;
- look-ahead **1 ms** = 48 muestras a 48 kHz, reportadas por `getLatency()`;
- release **80 ms**, ataque ligado al look-ahead, sin release adaptativo;
- true-peak desactivado (coste) y safety-clip interno desactivado;
- tras el limitador se aplica `fx::SafetyClip`, un soft-clip estático de codo
  0.95 y techo 1.0. Está por debajo del techo del limitador, así que en
  operación normal es transparente: sólo contiene NaN/Inf o un fallo de código.

El soft-clip de `synth::MasterLimiter` queda como referencia histórica y se
sigue probando, pero ya no está en el camino del `SynthProcessor`.

## Motivos

- El techo de −1 dBFS deja margen para el safety clip y para el redondeo del
  dispositivo, sin renunciar a nivel.
- El look-ahead de 1 ms es el mínimo que evita que un transitorio aislado pase
  antes de que el detector lo vea; 48 muestras son un coste aceptable dentro de
  los 240 muestras totales.
- Desactivar true-peak mantiene el coste bajo; el techo de muestra es lo que
  exige el test. Si en el futuro se mide inter-sample peak, se activa y se
  documenta el coste.
- Mantener un safety clip estático después del limitador no cuesta CPU y evita
  que un NaN produzca un clic o un valor no finito en el dispositivo.

## Consecuencias

- La latencia del instrumento sube 1 ms; se publica en la consola y en el
  resumen.
- Los picos fuertes ya no se comprimen con la curva del soft-clip: el limitador
  baja la ganancia con release, lo que puede oírse como pumping en material muy
  sostenido. El release de 80 ms es el compromiso elegido; la escucha humana
  decide si hay que subirlo.
- El safety clip no es un limitador: si el limitador fallara, la señal se
  comprimiría por encima de 0.95, no se recortaría dura.
- El test de la cadena completa comprueba que ningún pico supera 0.9 con 32
  voces, seis efectos activos y drive al máximo.
