# Estado de la sesión

Registro de una sesión larga de trabajo por bloques. Se reescribe después de
cada bloque, manteniendo el historial para que una sesión cortada no pierda
contexto. No sustituye a la verificación humana: los puntos que requieren oído
o vista se marcan como pendientes en `docs/ui-checklist.md` y
`docs/listening-checklist.md`.

## Estado general

- Build Release con Qt: verde.
- `ctest --preset vs2026-release`: **107/107** en verde (antes 99).
  - Recuento real: **104 casos de Catch2** (`drumas_tests`), **20 slots de
    QtTest** en `qt_ui_tests`, más 2 comprobaciones de CMake
    (`engine_has_no_qt`, `qml_theme_singleton`).
- Árbol de trabajo: limpio tras cada commit de bloque.
- Sin push; commits temáticos Conventional Commits sin trailers.

## Bloque 0 — Orden previo (terminado)

`git status` limpio y `ctest` 99/99 antes de tocar nada. Estado real de lo
pedido en la sesión anterior, con evidencia:

| Punto | Estado | Evidencia |
|---|---|---|
| Porcentajes ×100 y LCD `VOICES`/`XRUNS` | completo | `src/ui/qt/ValueFormat.cpp`; commit `8ca3fe0`; test `parameterValuesUseTheirUnitAndScale` |
| Look 3D de botones y perillas | completo | commit `5476764`; `ChordButton.qml`, `Knob.qml`, `Screw.qml`, `ThemeAmber.qml` |
| Piano en pantalla + teclas A W S E D F | completo | commit `8ccaf8b`; `PianoKeyboard.qml`; tests de mapeo y click |
| Ajustes persistentes | completo | commit `7d6b200`; `Settings.cpp`; ADR 0014; tests de ida/vuelta y archivo corrupto |
| Pads de acorde personalizables | completo (7 pads visibles) | `ChordEditorDialog.qml`, `EngineBridge::setChordSlot`; el octavo slot existe en el esquema pero no tiene botón |
| `LATCH` | completo | `EngineBridge` + `latchHoldsTheChordUntilAnotherOrDisabled` |

Nada de esto se reimplementó en este bloque; se retoma en el bloque 12 de la
lista si queda tiempo.

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

## Bloque 5 — Banco de patches y archivos JSON (terminado)

Hecho:

- `PatchFile.h/.cpp`: documento versionado (`schemaVersion: 1`) con `name`,
  `category` y `params` usando nombres legibles del registro. Escritura atómica
  con `QSaveFile`, carga tolerante, nombres desconocidos ignorados, valores
  clampeados, versión futura rechazada. Carpeta
  `%APPDATA%\Drumaspro\patches` (o `DRUMAS_PATCHES_PATH` para tests).
- Banco ampliado de 3 a 15 patches con categoría: Pad, Pluck, Keys (los
  originales) + Warm Pad, Glass Pad, Dark Drone, Soft Pluck, Pizz Synth, Lead
  Saw, Square Lead, Organ Drawbar, Electric Keys, Sub Bass, Air Texture y Brass
  Stab. `Patch` gana `category` (string_view, sigue siendo trivialmente
  copiable).
- Motor: `CommandType::SelectPatchData` + cola de retirada de `Patch*`. El
  hilo de audio copia el patch y devuelve el puntero; el control lo libera en
  `refreshStatus()`. Sin asignaciones en el callback.
- `EngineBridge`: `loadUserPatch`, `saveCurrentPatchToFile`,
  `userPatchDirectory`, `userPatchFiles`, `deleteUserPatch`,
  `duplicateUserPatch`, `patchInfo`/`patchCount`, `currentPatchName/Category`,
  `userPatchActive` y señal `patchChanged`.
- UI: `PatchBrowser.qml` (LCD nombre/categoría, PREV/NEXT, filtro por categoría)
  sustituye al dial de patch; la escala pasa a un `Selector` en la cabecera del
  panel SYNTHESIZER. `SETUP > PATCH FILES` con SAVE/LOAD/DUP/DEL y estado.
- Capturas regeneradas a 1280×720 y 960×600; el navegador y el selector de
  escala caben sin desplazar `SETUP` fuera de la ventana.

Tests añadidos: ida y vuelta completa (incluido `lfo.pitchDepth`), archivo
corrupto, versión futura, parámetros desconocidos, clamping de cutoff, categoría
obligatoria en cada patch de fábrica y navegador (15 opciones, NEXT y filtro).

Picos medidos del acorde de 4 notas (velocity 0.8, 4 s, techo del limitador
0.891): Pad 0.298; Pluck 0.253; Keys 0.126; Warm Pad 0.317; Glass Pad 0.154;
Dark Drone 0.647; Soft Pluck 0.042; Pizz Synth 0.218; Lead Saw 0.255; Square
Lead 0.328; Organ Drawbar 0.267; Electric Keys 0.142; Sub Bass 0.767; Air
Texture 0.332; Brass Stab 0.208. Ninguno alcanza el limitador; el paso máximo
mayor es 0.372 (Air Texture, contenido de ruido), sin discontinuidades.

A medias / limitaciones:

- El patch de usuario se aplica al motor pero no se añade al navegador de
  fábrica: el LCD del acorde muestra `USER / <categoría>` y el navegador sigue
  en el último patch de fábrica.
- `SAVE` usa el nombre actual (de fábrica o del patch cargado); no hay campo de
  texto para renombrar antes de guardar. `DUP` añade " Copy".
- `Soft Pluck` queda claramente más bajo que el resto (pico 0.042): es
  intencionado como pluck suave, pero puede requerir subir MASTER.
- El filtro del navegador se controla con un botón cíclico; no hay lista
  desplegable de categorías.

Verificación humana pendiente: timbre y balance de los doce patches nuevos,
recorrido del filtro, que el patch cargado sea el que suena y que los archivos
sobrevivan a un reinicio. Listas concretas en `docs/listening-checklist.md` y
`docs/ui-checklist.md`.

## Bloque 1 — ChordOn/ChordOff reales (terminado)

Hecho:

- `core::Command` gana `group` (1..31), `notes` (4 bytes empaquetados) y
  `noteCount`, más `CommandType::ChordOn`/`ChordOff` y los helpers
  `chordOn`, `chordOff`, `chordNoteAt`, `kMaxChordNotes = 4`. Sigue siendo
  trivialmente copiable.
- `VoicePool::chordOn` reserva **todas** las voces antes de arrancar ninguna:
  el acorde es atómico y todas sus notas empiezan en la primera muestra del
  bloque. Un acorde de más de cuatro notas o con grupo 0 se rechaza entero.
- Cada voz lleva una máscara de grupos (`uint32_t`, un bit por pad). Si una nota
  ya la mantiene otro acorde, el nuevo se une a esa voz en vez de reatacarla, y
  `chordOff` solo retira su bit: la nota compartida sigue sonando hasta que la
  suelte el último acorde.
- Las notas sueltas (piano, consola) van sin grupo y nunca reutilizan ni liberan
  una voz de acorde.
- Robo: primero voces en release (la más antigua), después la más silenciosa y
  a igualdad la más antigua; la voz reutilizada entra con fade-in de 2 ms.
- El puente ya no expande acordes: `bridge::ChordExpander` emite un solo
  comando y `EngineBridge` usa un `group = fila + 1`. `noteRefCounts_` queda
  solo para pulsaciones repetidas del piano.
- ADR [0015](adr/0015-chord-groups-and-voice-stealing.md).

Tests añadidos (7): arranque atómico en la primera muestra, C→Am con notas
compartidas sin reataque ni corte, `ChordOff` por grupo con varios acordes
mantenidos, notas sueltas que no roban voces de acorde, robo finito sin clics
(`largestStep < 0.5` con 32 voces), rechazo de acordes de más de cuatro notas y
continuidad de waveform en el cambio de acorde (`steady/switch/release`
comparados con el umbral de siempre).

A medias / limitaciones:

- El límite es 4 notas por acorde; no hay acordes de 5–6 notas (no los genera
  `src/music` hoy).
- El fade-in de 2 ms solo actúa al reutilizar una voz que sonaba; un ataque
  normal no se suaviza.
- `ChordOn` no lleva un id de "pulsación": dos pulsaciones del mismo pad sin
  soltar no se distinguen, igual que antes (el puente filtra la segunda).

Verificación humana pendiente: que el cambio C→Am no se oiga como reataque y
que el latch no deje notas colgadas con el nuevo camino; ver
`docs/ui-checklist.md` y `docs/listening-checklist.md`.

## Bloques 6 y 7

No iniciados: la sesión se cerró tras el bloque 5. El diseño para el diseñador
con IA y los extras (sampler, visor, atajos, MIDI) queda como propuesta de la
próxima sesión, sin código a medias.

## Resumen de la sesión

Hecho y verificado automáticamente:

- Bloques 0–5 completos, cada uno con build Release, `ctest` 99/99 y commits
  temáticos (sin push).
- UI: formato por unidad, LCD del motor, layout, skin 3D vintage, piano en
  pantalla, teclado del ordenador, ajustes persistentes, pads manuales, latch,
  panel SETUP, navegador de patches y archivos JSON de patch.
- Motor: `SelectPatchData` con retirada segura de punteros; banco de 15 patches
  con categorías; picos medidos por debajo del limitador.
- Documentación: README, `docs/ui.md`, `docs/ui-checklist.md`,
  `docs/listening-checklist.md` y ADR 0014 (settings/patches JSON).

A medias / deuda técnica:

- Bloques 6 (AI Patch Designer) y 7 (sampler, visor, atajos, MIDI) no
  empezados; no hay ADR de IA porque no hay módulo que documentar.
- El patch de usuario no entra en el navegador de fábrica y `SAVE` no permite
  renombrar antes de guardar.
- La escala de UI sigue siendo tipográfica, no geométrica.
- El octavo pad existe en el esquema pero no en la UI.
- El diálogo SETUP no refresca la etiqueta MANUAL/DIATONIC hasta reabrirlo.

Pendiente de verificación humana (no automatizable):

- Escucha de los quince patches y de cada efecto; listas en
  `docs/listening-checklist.md`.
- Aspecto real de las tres pieles, acentos y escala 125 % en Windows nativo;
  lista en `docs/ui-checklist.md`.
- Persistencia real de settings/patches tras cerrar y reabrir la aplicación y
  XRUNS con dispositivo de audio real.

Propuesta para la próxima sesión (5B + bloques pendientes):

1. Efectos restantes de 5B (flanger, phaser, tremolo, vibrato, lo-fi, shimmer,
   EQ) con sus tests de alias/estabilidad.
2. `ChordOn`/`ChordOff` reales con release solapado en el motor, en vez de
   expandir acordes a notas sueltas en el bridge.
3. Delay sincronizado por BPM y arpegiador.
4. Joystick y entrada MIDI USB detrás de una interfaz propia.
5. Diseñador de patches con IA opt-in: primero verificar la API real del
   proveedor y el almacenamiento de la clave (DPAPI/Credential Manager), luego
   cliente Qt Network fuera del motor, validador contra el registro y mock
   local para tests. No empezar por la UI.
