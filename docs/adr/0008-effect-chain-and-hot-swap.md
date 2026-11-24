# ADR 0008 — Cadena de seis slots y cambio de tipo en caliente

- **Estado:** aceptado (Etapa 5A)
- **Fecha:** 2026-11-24

## Contexto

La Etapa 5A necesita seis efectos en serie, con bypass y mezcla independientes,
y poder cambiar el tipo de un slot sin bloquear el hilo de audio ni destruir
objetos en él. Reverb, delay y chorus no se pueden construir sin asignar memoria,
y la reverb y el delay tienen colas que no deben cortarse con un clic.

## Decisión

`fx::EffectChain` posee los efectos actuales. El hilo de control:

1. crea el efecto con `fx::createPreparedEffect()` (construye y llama a
   `prepare()`, donde están todas las asignaciones);
2. envía `core::swapEffect(slot, dirección)` por la cola SPSC existente;
3. conserva la propiedad hasta que el audio lo confirma, y destruye los efectos
   que la cadena devuelve por `takeRetired()`.

El hilo de audio:

1. adopta el puntero;
2. cruza el efecto viejo y el nuevo durante 20 ms;
3. devuelve el viejo por `core::SpscQueue<IEffect*, 16>`.

`core::Command` sólo gana dos campos (`slot` y `object`), así que sigue siendo
trivialmente copiable. No se usa `std::shared_ptr` ni `unique_ptr` en el camino
de audio.

La latencia de cada slot se fija al máximo de los efectos del rack al preparar:
con drive presente, 32 muestras por slot. Así un cambio filtro→drive no cambia
la latencia del camino seco a mitad del crossfade. El coste es 4 ms para los
seis slots; el limitador añade 1 ms.

## Alternativas descartadas

- **Instancias preasignadas de todos los tipos en todos los slots.** Evita el
  hot swap, pero multiplica la memoria: una reverb FDN de 16 líneas y un delay
  de 2 s no son baratos, y preparar seis reverb + seis delays + seis chorus
  cuesta más que preparar el efecto que se usa. También obliga a resetear el
  estado al cambiar de tipo.
- **Construir en el hilo de audio.** Viola la regla de cero asignaciones.
- **Borrar el efecto viejo en el callback.** `delete` puede bloquear y es una
  violación explícita de `docs/rt-safety.md`.
- **Un lock alrededor de la cadena.** El callback no puede esperar a la UI.

## Consecuencias

- La cola de retiro tiene capacidad para 16 efectos; si el control deja de
  drenarla, el audio conserva hasta 8 punteros en una zona fija y, en el caso
  patológico, prefiere no liberar antes que liberar en el callback. La consola
  drena cada 5 ms.
- El cambio de tipo no es instantáneo: 20 ms de crossfade. Es el precio de no
  tener clic.
- La cadena no se puede leer desde el hilo de control mientras suena. La consola
  mantiene un espejo de lo que ha enviado; la verdad de audio vive en el
  procesador. La latencia total sí se publica con un atómico.
- El retiro de efectos al cerrar la app queda pendiente de drenar; el destructor
  de `EffectChain` los libera con el stream ya parado.
