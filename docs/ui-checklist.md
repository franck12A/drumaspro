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

- [ ] El estado del motor pasa a `ONLINE`; XRUNS permanece en cero al dejar la
  interfaz abierta y mover controles.
- [ ] Cambiar patch, tonalidad y modo actualiza el contenido mostrado sin
  desincronizar los modelos.
- [ ] Mantener pulsado cada botón de acorde produce el acorde indicado y al
  soltarlo se oyen releases limpios; el LED acompaña tanto el hold del botón
  como el de las teclas 1–7.
- [ ] Las teclas 1–7 producen los mismos acordes; auto-repeat no dispara notas
  adicionales y los controles enfocados conservan sus teclas.
- [ ] El medidor responde al nivel de salida y cae gradualmente al terminar el
  sonido; el LCD indica por separado las voces activas.
- [ ] Mover cada knob cambia el parámetro esperado sin saltos audibles.
- [ ] Activar/bypassear los seis slots y cambiar tipos/mezcla no produce clics
  ni bloquea la interfaz.
- [ ] Bloquear o arrastrar la ventana no causa glitches audibles ni deja el
  motor en un estado incoherente.
- [ ] En el tema predeterminado, el panel gris-beige, los marcos negros, la
  serigrafía, las perillas estriadas, los selectores y LEDs son legibles; el
  tema ámbar conserva contraste equivalente.
- [ ] En 1280×720 y 960×600 no hay texto cortado, glifos vacíos, controles
  solapados ni contenido fuera de la ventana.
- [ ] El espacio del joystick y del secuenciador se percibe como reservado; no
  aparenta ser un control funcional.

Anota sistema operativo, dispositivo/backend, frecuencia y tamaño de bloque,
resultado de XRUNS, incidencias audibles/visuales y captura de la ventana
nativa. Las capturas automatizadas
[`drumaspro-default-1280x720.png`](./screenshots/drumaspro-default-1280x720.png),
[`drumaspro-default-960x600.png`](./screenshots/drumaspro-default-960x600.png) y
[`drumaspro-amber-1280x720.png`](./screenshots/drumaspro-amber-1280x720.png) y
[`drumaspro-amber-960x600.png`](./screenshots/drumaspro-amber-960x600.png) no
sustituyen esta comprobación.
