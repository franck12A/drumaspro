# ADR 0013 — UI Qt/QML opcional y aislada del motor

- **Estado:** aceptado
- **Fecha:** 2026-11-25

## Contexto

La aplicación necesita una interfaz de escritorio con controles de sintetizador,
acordes, patches y cadena de efectos, sin introducir Qt en el motor ni en la
consola existente. Las actualizaciones de interfaz tampoco deben bloquear el
hilo de audio.

## Decisión

La UI se implementa en `src/ui/qt/` con Qt 6.8.3 y QML. El target
`drumaspro_qt` se configura solo si `DRUMAS_BUILD_UI=ON`; la opción está
desactivada por defecto. La consola conserva el ejecutable
`drumaspro_console`.

`EngineBridge` es el límite QObject de la aplicación. Lee el estado atómico a
40 Hz, publica cambios de control mediante la cola de comandos existente y
expone modelos de parámetros, patches, cadena y acordes. La lógica musical y
la expansión de acordes permanecen en módulos C++ sin Qt. Un acorde se traduce
a un batch todo-o-nada de NoteOn o NoteOff individuales.

CTest contiene una comprobación que falla si el motor o sus interfaces de
enlace adquieren dependencia de Qt. La prueba UI usa Qt Quick `offscreen` y
genera una captura de diagnóstico; la revisión nativa y la escucha siguen
siendo humanas.

## Consecuencias

- La consola, el motor y la CI no necesitan Qt.
- Los fallos o retrasos del hilo de UI no introducen esperas en el callback.
- La UI puede leer valores de estado ligeramente obsoletos, aceptable para
  medidores y etiquetas.
- La implementación actual agrupa notas de acorde al encolarlas, pero no define
  comandos de dominio `ChordOn`/`ChordOff` ni asignación de voces por grupo con
  una política de release solapado; eso queda para la Etapa 5B.
