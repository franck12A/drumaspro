# ADR 0004 — Osciladores limitados en banda

- **Estado:** aceptado (Etapa 4)
- **Fecha:** 2026-11-24

## Contexto

La voz sustractiva necesita seno, triángulo, sierra y pulso con ancho ajustable,
sin alias audible y sin asignaciones en el callback. El coste importa: 32 voces
× 2 osciladores × hasta 4 capas de unison son 256 osciladores en el peor caso.

Se evaluaron las dos primitivas de DSPark 1.7.0 antes de escribir código propio.

### `dspark::Oscillator`

- PolyBLEP de dos puntos para sierra y cuadrada, triángulo por integración con
  fugas, seno con `fastSin`.
- Contrato real-time correcto: `prepare()` construye la tabla compartida de
  minBLEP en el hilo de setup; después, todo es aritmética por muestra y no
  asigna. Documentado como no thread-safe, que es lo que necesitamos (un
  oscilador pertenece a una voz del hilo de audio).
- **Limitación dura:** la cuadrada está fijada al 50 % de ciclo. No hay ancho de
  pulso, y el ancho es uno de los parámetros del instrumento.
- Calidad medida con la métrica de este proyecto (ver abajo): una sierra
  PolyBLEP equivalente queda en **−35,8 / −33,0 / −31,1 dBc** a 1 / 1,5 / 3 kHz.
  Es una mejora clara sobre la sierra ingenua, pero audible en notas agudas y
  lejos del estándar que se quiere para el producto.

### `dspark::WavetableOscillator`

- Tablas con mipmaps, interpolación Hermite y crossfade cuadrático: rechazo de
  alias excelente por construcción.
- **No encaja en la arquitectura de voces:** cada instancia guarda su propio
  `std::vector` con las tablas (12 niveles × 2048 muestras). Con 256 osciladores
  serían decenas de MB y una construcción con DFT por instancia en `prepare()`.
- El ancho de pulso exigiría reconstruir la tabla (DFT offline), así que no es
  parametrizable a frecuencia de audio.

## Decisión

Oscilador propio (`synth::BandLimitedOscillator`), detrás de `IOscillator`:

- **Seno:** aproximación minimax de grado 9 (`dspark::fastSin`, error máximo
  ~4e-6), la misma que usa DSPark. La función se usa como primitiva matemática;
  ningún tipo de DSPark aparece en la API pública.
- **Sierra y pulso:** la discontinuidad se corrige con la tabla compartida
  **minBLEP** de DSPark (`MinBlepTable`, licencia MIT), construida una sola vez
  en el primer `prepare()` del hilo de setup. El pulso es la diferencia de dos
  sierras limitadas en banda, una desplazada por el ancho; a 0,5 sale una
  cuadrada, a otros anchos un pulso sin continua.
- **Triángulo:** integración con fugas de la cuadrada limitada en banda,
  normalizada a la amplitud de régimen permanente de cada frecuencia (curva de
  tipo analógico, la misma idea que DSPark).
- **Bloqueador de continua** de un polo (~4 Hz) por oscilador. El residuo
  minBLEP tiene área no nula; al periodizarse en una sierra de 7 kHz dejaba
  +0,57 de offset medido. El bloqueador lo elimina y deja el pulso sin continua
  a cualquier ancho.
- Coste: 64 sumas por discontinuidad (una por ciclo en sierra; dos en pulso y
  triángulo), estado de ~300 bytes por oscilador y cero asignaciones tras
  `prepare()`.

## Métrica de alias

Render offline de 65 536 muestras a 48 kHz, ventana Blackman-Harris de 4
términos, FFT, y suma de energía fuera de ±8 bins de cada armónico. Se cuenta
solo la banda audible (< 20 kHz): un armónico que cae 3 Hz por encima de Nyquist
pliega a 3 Hz por debajo y es inaudible; contarlo haría que un oscilador limpio
pareciera malo por un artefacto que nadie puede oír. La sierra ingenua es el
control que demuestra que la métrica detecta alias de verdad.

## Resultados medidos (48 kHz, 65 536 muestras)

| Señal | 1 kHz | 1,5 kHz | 3 kHz |
|---|---|---|---|
| Sierra minBLEP | −94,8 dBc | −92,2 dBc | −86,1 dBc |
| Sierra PolyBLEP (referencia) | −35,8 dBc | −33,0 dBc | −31,1 dBc |
| Sierra ingenua (referencia) | −15,0 dBc | −13,1 dBc | −10,3 dBc |

| Pulso minBLEP | 1 kHz | 3 kHz |
|---|---|---|
| ancho 0,5 | −104,1 dBc | −99,3 dBc |
| ancho 0,25 | −101,8 dBc | −97,0 dBc |
| ancho 0,1 | −95,1 dBc | −93,7 dBc |

Afinación: el pico espectral coincide con la frecuencia pedida dentro de
±0,04 céntimos entre 110 Hz y 7 kHz en las cuatro formas.

Los tests fijan umbrales con margen sobre estos valores: −75 dBc para la sierra,
−80 dBc para pulsos de ancho ≥ 0,25 y −75 dBc para ancho 0,1, más una mejora de
al menos 50 dB frente a la sierra ingenua.

## Consecuencias

- El suelo de alias depende de la ventana de la tabla minBLEP; una tabla más
  larga bajaría algo el suelo a cambio de CPU.
- Si en el futuro hace falta un banco de tablas compartido entre voces (por
  ejemplo, para samples o wavetables de usuario), el sitio natural es un
  `WavetableBank` construido en `prepare()`; la interfaz `IOscillator` no
  cambiaría.
- El bloqueador de continua introduce un polo muy grave que no afecta a la
  banda de audio, pero conviene recordarlo al medir respuesta a 10-20 Hz.
