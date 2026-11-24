# ADR 0009 — Colas, bypass, mezcla y compensación de latencia

- **Estado:** aceptado (Etapa 5A)
- **Fecha:** 2026-11-24

## Contexto

Un delay o una reverb no se pueden apagar en seco sin cortar la cola. Además,
el camino seco de un slot con un efecto retardado (drive sobremuestreado) no
puede mezclarse con el wet sin compensar el retardo, o aparece un peine.

## Decisión

### Mezcla en el slot

Cada efecto procesa **100 % wet**. El slot aplica:

```
salida = dry_retardado * (1 - mix * activo) + wet * (mix * activo)
```

`activo` es 1 con bypass desactivado y 0 con bypass activo; se rampa en 20 ms.
`mix` se rampa en 10 ms. El dry se retarda la latencia del slot, de modo que en
`mix = 0.5` dry y wet están alineados. Un slot bypass sigue retardando el dry:
el bypass es exacto y conserva la latencia del slot (test muestra a muestra).

### Cola al activar bypass

Al conmutar a bypass, el wet baja a cero y el efecto **sigue procesando** hasta
que se cumple una de estas condiciones:

- la salida wet lleva 100 ms por debajo de −80 dB, o
- se agota el presupuesto `tailSamples()` del efecto (30 s reverb, 20 s delay,
  100 ms filtro, 60 ms chorus).

Entonces se resetea y se deja de procesar. El dry sigue sonando durante toda la
cola. Un cambio de tipo no espera la cola: hace crossfade de 20 ms y retira el
viejo, porque el objetivo es cambiar el efecto, no conservar su cola.

### Latencia

`IEffect::latencySamples()` es la latencia real del efecto a la frecuencia base.
El limitador reporta 48 muestras (1 ms con look-ahead a 1 ms). La cadena suma
las latencias de los slots y `SynthProcessor` publica la suma con el limitador
en un atómico que la consola puede leer sin carrera.

La cadena reserva el máximo latencia del rack en todos los slots al preparar
(32 muestras si hay drive). Sin esa reserva, cambiar un filtro por un drive
movería el dry 32 muestras a mitad del crossfade y podría chasquear. El coste
es 192 muestras (4 ms) en la cadena por defecto.

## Consecuencias

- El primer bloque tras un bypass no es exactamente seco: la rampa de 20 ms es
  intencionada.
- Un efecto con una cola muy larga y bypass activo puede consumir CPU hasta
  30 s. Es el precio de no cortar la cola; el umbral de −80 dB la corta antes en
  la práctica.
- El dry de un slot con latencia no es el dry instantáneo; es el mismo material
  retrasado. Esto es lo correcto para mezclar, pero cambia la latencia total del
  instrumento.
- La prueba de cola silenciosa usa FTZ/DAZ (`core::DenormalGuard`) y compara el
  tiempo de bloques cercanos al inicio y tras 5 s de silencio; no mide una
  máquina, pero detecta el crecimiento por denormales.
