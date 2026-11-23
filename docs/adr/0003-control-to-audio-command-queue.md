# ADR 0003 — Comunicación control↔audio por cola lock-free y atómicos

- **Estado:** aceptado (Etapa 2)
- **Fecha:** 2026-11-23

## Contexto

La UI (o, en esta etapa, el teclado de consola) produce eventos: nota on, nota
off, cambios de parámetro. El hilo de audio los consume al principio de cada
bloque. Un mutex compartido, un `std::vector` de eventos o una variable
compartida sin sincronizar producirían bloqueos y carreras en el peor sitio
posible.

## Decisión

- **Control → audio:** `core::SpscQueue<core::Command, 512>` — cola de un
  productor y un consumidor, sin locks, sin asignaciones, capacidad fija
  (potencia de dos, capacidad usable 511). Si se llena, el evento se descarta y
  se cuenta; el hilo de control nunca espera.
- **Audio → control:** palabras `std::atomic` sueltas (`std::memory_order_relaxed`)
  para contadores y medidores: bloques, frames, glitches, callbacks pasados de
  tiempo, tiempo máximo de callback, voces activas.
- Los comandos se drenan **al principio de cada bloque** y se pasan al procesador
  como `std::span<const core::Command>`, así el procesador nunca ve la cola.

El contrato de hilos de la cola está verificado contra
`docs/threading.md` del tag de DSPark (un productor, un consumidor, wait-free,
sin asignaciones), y el test de dos hilos lo comprueba en la práctica.

## Motivos

- Latencia determinista: el peor caso del hilo de audio no depende de la UI.
- Sin prioridad invertida: no hay ningún camino por el que el audio espere a la
  UI.
- El tamaño fijo permite razonar sobre el consumo por bloque y evita el
  allocador en el callback.
- `Command` es trivialmente copiable (`static_assert` en el tipo), requisito
  duro para que la transferencia no sea una carrera de datos.

## Consecuencias

- Un evento que llega con la cola llena se pierde; en notas es aceptable y queda
  contado (`commandsDropped`). Si en el futuro hace falta no perder ninguno, se
  añadirá un estado "última voluntad" por parámetro, no una cola más grande.
- La UI nunca lee estado de las voces: solo los atómicos publicados. Leer
  `VoicePool` desde otro hilo sería una carrera.
- Los medidores pueden ir un bloque por detrás. Para un indicador visual es
  irrelevante.
