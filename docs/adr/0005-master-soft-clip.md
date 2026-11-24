# ADR 0005 — Soft clip en la etapa maestra

- **Estado:** aceptado (Etapa 4)
- **Fecha:** 2026-11-24

## Contexto

La Etapa 3 pedía sustituir el recorte duro por un limitador o un soft-clip
seguro. Lo que quedó implementado fue literalmente
`std::clamp(x, -1, 1)`: un recorte duro, documentado como limitación en el
README. Con un pad de unison, un acorde de cuatro notas suma picos por encima de
la escala completa; el recorte duro convierte cada pico en una esquina, con
armónicos impares e intermodulación justo donde el acorde debería sonar limpio.

## Decisión

`synth::MasterLimiter`, un soft-clip sin estado:

- Por debajo de `kKnee = 0.7` la señal pasa **exacta** (sin color, sin
  reducción de ganancia).
- Por encima, `y = signo(x) * (0.7 + 0.3 * tanh((|x| - 0.7) / 0.3))`.
- La salida está acotada por 1 por construcción (`tanh` tiende a 1 sin
  alcanzarla), es continua y C1 en el codo (`tanh'(0) = 1`), monótona e impar.
- Un valor no finito se convierte en silencio: es un fallo aguas arriba y no
  debe llegar al dispositivo.

No se implementa un limitador con look-ahead en esta etapa: requiere línea de
retardo, detector de envolvente y curva de release, y su sitio natural es la
cadena de efectos de la Etapa 5, donde se puede medir junto con reverb, delay y
drive. Un soft-clip resuelve el techo ahora sin latencia añadida.

## Motivos

- **Techo garantizado** sin estado ni retardo: el test barre entradas hasta
  ±1e6 y comprueba que la salida nunca alcanza 1.
- **Transparencia en el rango útil:** los patches se calibran para que un
  acorde de cuatro notas ronde 0,3-0,6 de pico; el codo en 0,7 no los toca.
- **Continuidad:** el test comprueba que una rampa de entrada de 1e-4 no
  produce saltos de salida mayores que la propia rampa, que es la definición
  práctica de "sin clic" en una etapa estática.
- **Coste despreciable:** una comparación por muestra; `tanh` solo en los picos.

## Consecuencias

- Los picos fuertes se comprimen (pérdida de crest factor) en lugar de
  recortarse; el timbre se mantiene suave, pero no hay control de release ni
  de ganancia de compensación. Si un patch concreto resulta demasiado
  comprimido, se corrige en la mezcla del patch, no subiendo el master.
- Los tests de patch comprueban el techo real con acordes de cuatro notas; el
  benchmark mide el coste con el limitador activo.
- Un limitador con look-ahead puede sustituir a este sin tocar `SynthProcessor`
  más allá de una línea, porque la etapa es una función pura por muestra.
