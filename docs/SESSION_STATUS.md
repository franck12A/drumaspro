# Estado de la sesión

Registro de una sesión larga de trabajo por bloques. Se reescribe después de
cada bloque, manteniendo el historial para que una sesión cortada no pierda
contexto. No sustituye a la verificación humana: los puntos que requieren oído
o vista se marcan como pendientes en `docs/ui-checklist.md` y
`docs/listening-checklist.md`.

## Estado general

- Build Release con Qt: verde.
- `ctest --preset vs2026-release`: 99/99 en verde al cerrar el bloque 1.
- Árbol de trabajo: limpio tras cada commit de bloque.
- Sin push; commits temáticos Conventional Commits sin trailers.

## Bloque 0 — Orden previo (terminado)

- Los cambios pendientes de la sesión anterior (inspector de efectos, notas
  compartidas entre acordes, niveles de oscilador en vivo, docs de Qt y
  capturas) quedaron en dos commits temáticos.
- `ctest` completo en verde antes de empezar.
- Creado este archivo.

## Bloque 1 — Formato y layout (terminado)

Hecho:

- `src/ui/qt/ValueFormat.{h,cpp}`: formateo por unidad compartido entre el
  modelo y QML. Porcentajes 0..1 → `55 %`; Hz con 1–2 decimales solo por debajo
  de 100; ms enteros desde 10; dB/ct/oct/ratio sin ceros sobrantes.
  `EngineBridge::formatParameterValue(id, value)` y
  `EngineBridge::formatUnitValue(unit, value)` son `Q_INVOKABLE`.
- Las perillas (`Knob.qml`) y la franja de parámetros de efecto usan el mismo
  formateador; las etiquetas de mínimo/máximo del dial escalan los porcentajes.
- LCD del motor: `ENGINE`, `ONLINE/OFFLINE`, `VOICES n / XRUNS n`, con ancho
  ampliado para no truncarse en 960×600.
- `MASTER` movido del encabezado al panel SYNTHESIZER a la derecha de LFO; la
  fila de parámetros ya no deja hueco. Encabezado alineado por abajo para una
  línea base común. EFFECTS CHAIN dentro de un marco `ThemeAmber`.
- LED de estado en cada `EffectSlot`: rojo en bypass, verde activo, además del
  texto (halo leve).
- Tipografía: nuevo token `Theme.minimumFontSize` (8 px compacto, 9 px normal).
  Antes/después: `tinySize` 7/8 → 8/9, `smallTextSize` 8/9 → 9/10,
  `labelSize` 9/10 → 10/11, `rotaryLabelSize` 7/8 → 8/9.
- `RotarySelector` imprime las posiciones alrededor del dial cuando hay 2–4
  opciones (PAD/PLK/KEY, MAJ/MIN); con más opciones solo queda el rótulo bajo el
  dial. El dial se reduce lo justo para que el texto quepa sin recortarse.
- Test `parameterValuesUseTheirUnitAndScale` cubre % / Hz / ms / dB / ct / oct y
  la consulta por id.

A medias / limitaciones:

- Las posiciones impresas usan abreviaturas de 3–4 caracteres; si con más
  opciones se necesitan completas habrá que cambiar el control (previsto en el
  bloque 5).
- La franja del inspector reparte el ancho entre parámetros; con cuatro
  parámetros los valores quedan separados de su etiqueta. Es el diseño previo y
  no se tocó más allá del formato.

Verificación humana pendiente: nueva lista en `docs/ui-checklist.md`,
incluyendo porcentajes, LCD, posición de MASTER, LEDs, rótulos del dial y
tamaño mínimo.

## Bloques 2–7

Pendientes al escribir esta entrada.
