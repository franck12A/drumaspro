# Checklist de verificación humana de la UI

**Estado: pendiente.** Las capturas offscreen se generan en cuatro tamaños/temas
y los tests automatizados cubren la carga QML, pero aún se necesita una revisión
humana en una sesión Windows con dispositivo de audio. No marcar ningún punto
como verificado sin ejecutarlo.

## Ejecución

1. Instala/selecciona Qt 6.8.3 para MSVC x64 y configura con
   `cmake --preset vs2026 -DDRUMAS_BUILD_UI=ON`.
2. Compila `cmake --build --preset vs2026-release --target drumaspro_qt`.
3. Añade `.tools/Qt/6.8.3/msvc2022_64/bin` al `PATH` o despliega las DLL de Qt
   junto al ejecutable y ejecuta
   `build\vs2026\src\ui\qt\Release\drumaspro.exe`.
4. Comprueba que la ventana abre sin errores QML, que los textos y controles se
   ven correctamente y que el tamaño mínimo permite usar toda la pantalla.

## Comprobaciones manuales

- [ ] El estado del motor pasa a `ONLINE`; el LCD muestra voces activas y
  XRUNS, que permanece en cero al dejar la interfaz abierta y mover controles.
- [ ] Cambiar patch, tonalidad y modo actualiza el contenido mostrado sin
  desincronizar los modelos.
- [ ] Mantener pulsado cada botón de acorde produce el acorde indicado y al
  soltarlo se oyen releases limpios; el LED acompaña tanto el hold del botón
  como el de las teclas 1–7.
- [ ] Las teclas 1–7 producen los mismos acordes; auto-repeat no dispara notas
  adicionales y los controles enfocados conservan sus teclas.
- [ ] Mantener dos acordes con notas comunes (por ejemplo C y Em) no reataca la
  nota compartida ni la corta al soltar el primero.
- [ ] El medidor responde al nivel de salida y cae gradualmente al terminar el
  sonido; el LCD indica por separado las voces activas.
- [ ] Mover cada knob cambia el parámetro esperado sin saltos audibles.
- [ ] Activar/bypassear los seis slots y cambiar tipos/mezcla no produce clics
  ni bloquea la interfaz.
- [ ] Seleccionar cada slot actualiza la franja de parámetros con el tipo y los
  valores correctos; moverlos cambia el efecto sin saltos y el valor mostrado
  coincide con el oído.
- [ ] Bloquear o arrastrar la ventana no causa glitches audibles ni deja el
  motor en un estado incoherente.
- [ ] En el tema predeterminado, el panel gris-beige, los marcos negros, la
  serigrafía, las perillas estriadas, los selectores y LEDs son legibles; el
  tema ámbar conserva contraste equivalente.
- [ ] En 1280×720 y 960×600 no hay texto cortado, glifos vacíos, controles
  solapados ni contenido fuera de la ventana.
- [ ] La franja inferior no se confunde con un adorno: sus deslizadores
  responden al clic, al arrastre y a la rueda, y el slot seleccionado se
  distingue con el borde de acento.
- [ ] Los porcentajes se leen como `55 %`, `100 %` o `4.5 %`, nunca como
  `0.55 %`; Hz, ms y dB muestran solo los decimales necesarios.
- [ ] El LCD del motor dice `ENGINE` / `ONLINE` / `VOICES n / XRUNS n`, sin
  abreviaturas tipo `4V` ni `0XR`, y nada queda truncado en 960×600.
- [ ] `MASTER` está dentro del panel SYNTHESIZER a la derecha de LFO y la fila
  de parámetros no deja un hueco muerto a la derecha.
- [ ] La sección EFFECTS CHAIN está dentro de un marco coherente con el resto y
  el LED de cada slot es rojo en bypass y verde en activo.
- [ ] En los selectores de patch y escala se leen las posiciones impresas
  alrededor del dial; si se solapan con el dial o el rótulo, anótalo.
- [ ] Los textos más pequeños siguen siendo legibles a 960×600 (mínimo
  `Theme.minimumFontSize`) y no se cortan.

- [ ] Las teclas y botones se ven físicos: bisel claro arriba-izquierda,
  sombra abajo-derecha, sombra proyectada y hundimiento de ~2 px al pulsar;
  el hover es sutil y el LED del acorde enciende un halo suave.
- [ ] Las perillas muestran brillo especular, sombra sobre el panel y estrías
  marcadas; al girarlas no parpadean ni se descolocan.
- [ ] Los marcos SYNTHESIZER y EFFECTS CHAIN tienen tornillos en las esquinas,
  línea de bisel y degradado de panel sin que los tornillos tapen texto.
- [ ] En el tema ámbar el bisel y las sombras conservan contraste (no se ven
  como manchas negras ni pierden relieve).
- [ ] Al abrir la ventana y mover controles, la interfaz responde sin tirones
  visibles (la medición automatizada da ~3,2 ms por frame a 1280×720).
- [ ] El botón `KEYS` cambia la franja de acordes por el piano sin desplazar el
  resto de la ventana; a 1280×720 se ven tres octavas (C3..B5) y a 960×600 dos
  (C3..B4), con las etiquetas C3/C4/C5 legibles.
- [ ] Pinchar y arrastrar sobre el piano produce glissando (sin notas colgadas)
  y la velocity cambia según la altura del clic; las teclas se hunden y las que
  suenan quedan resaltadas.
- [ ] Mantener un acorde con 1–7 mientras está abierta la vista de piano
  resalta sus notas; al soltarlo desaparece el resaltado.
- [ ] `A W S E D F T G Y H U J K` suenan C..C; `Z`/`X` cambian la octava, el LCD
  `OCTAVE` la refleja y la nota limite no supera MIDI 127.
- [ ] El auto-repeat no dispara notas extra y, al enfocar un selector o al
  perder el foco de la ventana, no quedan notas sonando.

Anota sistema operativo, dispositivo/backend, frecuencia y tamaño de bloque,
resultado de XRUNS, incidencias audibles/visuales y captura de la ventana
nativa. Las capturas automatizadas
[`drumaspro-default-1280x720.png`](./screenshots/drumaspro-default-1280x720.png),
[`drumaspro-default-960x600.png`](./screenshots/drumaspro-default-960x600.png) y
[`drumaspro-amber-1280x720.png`](./screenshots/drumaspro-amber-1280x720.png) y
[`drumaspro-amber-960x600.png`](./screenshots/drumaspro-amber-960x600.png) no
sustituyen esta comprobación.
