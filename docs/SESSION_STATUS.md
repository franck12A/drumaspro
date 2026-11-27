# Estado de la sesión

Registro de una sesión larga de trabajo por bloques. Se reescribe después de
cada bloque, manteniendo el historial para que una sesión cortada no pierda
contexto. No sustituye a la verificación humana: los puntos que requieren oído
o vista se marcan como pendientes en `docs/ui-checklist.md` y
`docs/listening-checklist.md`.

## Estado general

- Build Release con Qt: verde.
- `ctest --preset vs2026-release`: 99/99 en verde al cerrar el bloque 4.
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

## Bloque 2 — Look 3D vintage (terminado)

Hecho:

- Botones (`ChordButton`, `ToggleButton`, `Selector`): bisel con gradiente luz
  arriba-izquierda / sombra abajo-derecha, línea interior, sombra proyectada que
  se reduce al pulsar (3 → 1 px), desplazamiento de 2 px y velo de hover.
- LED de acorde con halo de dos discos; LED de cada slot de efecto con halo
  rojo/verde además del texto.
- Perillas y selectores rotativos: sombra con `shadowBlur`, anillo de base con
  gradiente diagonal, estrías alternas clara/oscura, brillo especular radial y
  repintado al cambiar de tema. Los números de mínimo/máximo se movieron fuera
  del anillo y se limitan a perillas grandes (los diales compactos ya no los
  muestran, antes se dibujaban encima de la tapa).
- Marcos (`ThemeAmber`) y chasis: degradado vertical, luces de bisel y cuatro
  tornillos decorativos nuevos (`Screw.qml`). EffectSlot desactiva los tornillos
  por tamaño; el inspector también, para no tapar los controles.
- Rendimiento medido: repintado completo 1280×720 = **3,2 ms/frame** (~309 fps
  de margen) en backend software offscreen. Sin `layer.enabled`: los `Canvas`
  retienen su contenido y las decoraciones son rectángulos baratos; añadir capas
  habría costado memoria sin mejorar un coste ya muy por debajo de 16,6 ms.
- Capturas de ambos temas a 1280×720 y 960×600 regeneradas y revisadas.

Qué se ve (honesto):

- El tema gris-beige gana relieve claro: teclas con bisel, perillas con brillo
  arriba-izquierda y sombra suave; los tornillos se leen como puntos metálicos
  discretos en las esquinas de los marcos.
- El tema ámbar conserva el contraste; las teclas y el chasis quedan más
  "oscuros y cálidos" y los LEDs rojos destacan.
- Puntos débiles: el bisel es sutil a 960×600 (pocos píxeles por línea); los
  números de escala desaparecieron de las perillas pequeñas (decisión para no
  tapar la tapa); no hay mejillas de madera.

Pendiente de verificación humana: relieve y hover reales en pantalla nativa,
halo de LED encendido, tornillos que no tapen texto, sensación de respuesta.
Lista en `docs/ui-checklist.md`.

## Bloque 3 — Teclado de piano (terminado)

Hecho:

- `PianoKeyboard.qml`: 2 octavas desde C3 a 960×600 y 3 octavas desde 1100 px de
  ancho; teclas blancas/negras con bisel y gradiente, hundimiento de 2 px,
  resaltado por acento de las notas sonando, serigrafía C3/C4/C5.
- Clic/arrastre con glissando (NotaOff de la anterior antes de la siguiente) y
  velocity por altura vertical del clic (0.45–1.0).
- Vista conmutable `CHORDS`/`KEYS` con un botón en el panel SYNTHESIZER. Fue la
  única forma de mantener ambos a 1280×720 y 960×600 sin scroll; documentado.
- API nueva del puente, testeable: `noteOn`/`noteOff` (comparten los contadores
  de referencia con los acordes), `pianoOctave` (0..8), `shiftPianoOctave`,
  `semitoneForKey` y `midiNoteForSemitone` (clampeo 0..127).
- Teclado del ordenador: `A W S E D F T G Y H U J K` = C..C, `Z`/`X` octava,
  sin auto-repeat, sin robar teclas con foco en controles, libera notas al
  perder foco. Las teclas 1–7 siguen tocando acordes. LCD `OCTAVE` añadido.
- Capturas nuevas de la vista de piano a 1280×720 y 960×600.

Tests añadidos (dentro de `qt_ui_tests`, que la suite ejecuta como un solo
caso): mapeo de teclas a semitonos, octava y límites MIDI, noteOn/noteOff con
referencias compartidas acorde+piano, clic en tecla del piano (NoteOn/NoteOff),
mapeo del teclado del ordenador, auto-repeat, cambio de octava y resaltado de un
acorde mantenido.

A medias / limitaciones:

- La velocity por altura de clic solo aplica al ratón; el teclado del ordenador
  usa una velocity fija de 0.8 porque un teclado no tiene presión.
- El resaltado solo pinta notas dentro del rango visible (C3..B5 con tres
  octavas, C3..B4 con dos); los acordes actuales caen dentro, pero un patch
  futuro con notas más graves o agudas no se vería en el teclado.
- No hay entrada MIDI externa (bloque 7d, no realizado).

Verificación humana pendiente: sonido del piano y glissando, sensación de
velocity, resaltado y que la franja no se desplace al conmutar. Lista en
`docs/ui-checklist.md`.

## Bloque 4 — Ajustes persistentes (terminado)

Hecho:

- `Settings.h/.cpp`: interfaz propia sobre `QJsonDocument` (sin dependencia
  nueva). Archivo `%APPDATA%\Drumaspro\settings.json` con `schemaVersion`,
  escritura atómica con `QSaveFile` y carga tolerante: ausente/vacío/truncado/
  inválido → defectos; versión futura → rechazo completo; campos desconocidos →
  ignorados; valores fuera de rango → clamp.
- Ajustes: piel (default/ámbar/noche), 6 presets de acento (0 = propio de la
  piel + verde/ámbar/hielo/violeta/óxido), escala tipográfica 90/100/110/125,
  sensibilidad de perillas 50–150, curva de velocity (lineal/suave/dura), octava
  por defecto, última tonalidad/escala/patch, séptimas y latch.
- Tema noche con acento fósforo; la escala de UI multiplica la tipografía (la
  geometría se mantiene para no romper 960×600).
- Pads manuales por pad (raíz, 10 calidades de `src/music`, inversión, octava),
  editor con clic derecho o `SETUP > CHORD PADS > EDIT`, validador que rechaza
  voicings fuera de MIDI 0..127. Esquema con 8 slots, UI con 7 (octavo
  reservado). Séptimas globales.
- `LATCH` en el puente (sin tocar el motor) con LED: mantiene el acorde, otro lo
  reemplaza, apagarlo lo libera.
- Diálogo `SETUP` con el estilo del panel (marco, tornillos) y captura en
  `docs/screenshots/drumaspro-setup-1280x720.png`. Selector de dispositivo y
  buffer listados como pendientes, sin implementación a medias.
- `%APPDATA%` no se toca en tests: `DRUMAS_SETTINGS_PATH` apunta a un archivo
  temporal.

Tests añadidos: ida y vuelta de todos los campos, archivo corrupto/vacío/
truncado, campos desconocidos, versión futura, clamping, pads manuales vs
diatónicos, séptimas, latch y apertura de `SETUP`/editor por clic derecho.

A medias / limitaciones:

- La escala de UI es tipográfica, no un zoom de todos los controles.
- El octavo pad está en el esquema pero no tiene botón en la UI.
- El diálogo de ajustes no refresca la etiqueta MANUAL/DIATONIC de la lista
  hasta reabrirlo (el valor sí se guarda y el pad suena correcto).

Verificación humana pendiente: que la piel noche y los acentos se vean bien,
que la escala 125 % no recorte textos, latch audible sin notas colgadas y
persistencia real tras reiniciar. Lista en `docs/ui-checklist.md`.

## Bloques 5–7

Pendientes al escribir esta entrada.
