# ADR 0012 — Apagado seguro del motor

- **Estado:** aceptado
- **Fecha:** 2026-11-25

## Contexto

La cola SPSC puede contener comandos aceptados que el callback aún no aplicó
cuando se cierra el motor. `SwapEffect` transfiere la propiedad de un efecto
preparado al audio al encolarse; si nunca se procesa, ese objeto no pertenece a
la cadena y quedaría huérfano.

## Decisión

`AudioEngine::shutdown()` detiene el dispositivo, marca el stream como parado,
drena la cola en el hilo de control, resetea el procesador y finalmente libera
el dispositivo y el contexto. Al drenar, llama a
`IAudioProcessor::discardCommand()` para que la implementación libere recursos
propios de comandos no aplicados. `SynthProcessor` elimina el efecto de un
`SwapEffect` pendiente.

`AudioGraph::popPendingCommand()` solo se usa con el stream detenido y el
productor quiescente. La cola sigue siendo SPSC; el método no habilita a un
segundo consumidor concurrente.

## Consecuencias

- El apagado no destruye recursos en el callback.
- La capa `engine` no depende de tipos concretos de efectos.
- La aplicación debe impedir que el productor encole nuevos comandos durante
  `shutdown()`.
- Los procesadores que incorporen comandos con recursos deben implementar
  `discardCommand()`; los comandos de valor predeterminado no necesitan acción.
