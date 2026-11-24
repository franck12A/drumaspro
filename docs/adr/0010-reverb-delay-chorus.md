# ADR 0010 — Reverb, delay y chorus: DSPark frente a implementación propia

- **Estado:** aceptado (Etapa 5A)
- **Fecha:** 2026-11-24

## Contexto

La etapa necesita reverb (tamaño/decay, damping, pre-delay), delay con feedback
estable, ping-pong y filtro en el lazo, y chorus con interpolación de calidad,
voces, profundidad, velocidad y estéreo ancho. Todo debe quedar detrás de
`fx::IEffect` y no introducir dependencias nuevas.

## Decisión

- **Reverb:** `dspark::AlgorithmicReverb` (v1.7.0, MIT), envuelta en
  `fx::ReverbEffect`. Se usa calidad **Full** y tipo Hall; el wrapper fija el
  mix interno a 1 porque la mezcla vive en el slot. `setDecay`, `setSize`,
  `setDamping` y `setPreDelay` se mapean a la API pública. La calidad Eco queda
  como palanca documentada si el benchmark se acerca al límite.
- **Chorus:** `dspark::Chorus` (MIT), envuelto en `fx::ChorusEffect`; mix
  interno a 1, 4 voces, spread estéreo y centro a 12 ms. No se añade Signalsmith
  DSP: sería una dependencia nueva para una función que DSPark ya cubre.
- **Delay:** implementación propia (`fx::DelayEffect`) sobre
  `dspark::RingBuffer`, con interpolación Hermite, cross-feedback real para
  ping-pong y un pasa-bajos de un polo en el lazo. El `Delay` de DSPark no
  expone el lazo para cruzar canales, y el ping-pong es un requisito explícito;
  usar dos instancias y un lazo externo habría duplicado estado y hecho menos
  claro el límite de estabilidad.
- **Drive:** implementación propia sobre `dspark::Oversampling` 2× (calidad
  Medium, 63 taps) y `dspark::StateVariableFilter`; un pasa-altos fijo a 30 Hz
  antes de la saturación, un pasa-bajos de tono después y compensación RMS
  automática más un trim de salida.
- **Filtro de cadena:** `fx::FilterEffect` sobre `StateVariableFilter`, con
  LP/HP/BP. No se reutiliza `synth::ResonantLowPassFilter` porque es sólo LP y
  vive en el módulo del sintetizador.
- **Limitador:** ver ADR 0011.

## Motivos

- Las tres implementaciones de DSPark son MIT, ya están en el mismo tag que el
  Core vendorizado y tienen contratos explícitos de `prepare()` (asigna) y
  `processBlock()` (sin asignar). Reverb y chorus son lo bastante grandes como
  para que escribirlos de nuevo añada riesgo sin ventaja.
- El delay y el drive son pequeños, y la variante propia es la que cumple los
  requisitos de ping-pong y de control del oversampling sin pelear con una API
  pensada para otro lazo.
- No se añade ninguna dependencia: los headers de Effects se vendorizan del
  mismo commit que el resto de DSPark.

## Consecuencias

- La reverb es el coste dominante del rack. El benchmark con seis insertos y 32
  voces da 38–45 % del presupuesto; si en otra máquina se acerca al 60 %, la
  primera palanca es `Quality::Eco`, después una FDN propia de 8 líneas.
- El delay propio debe mantener el feedback acotado a 0.95 y comprobar
  estabilidad con una entrada impulsiva de 10 s; el test lo hace.
- El chorus de DSPark mezcla internamente; se configura a 1 y se prueba que la
  señal sigue siendo finita y sin saltos.
- La calidad de la reverb, el carácter del drive y el ancho del chorus no se
  pueden validar con tests; quedan en la checklist de escucha.
