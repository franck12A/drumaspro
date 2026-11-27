# Interfaz de escritorio Qt/QML

La aplicación de escritorio (`drumaspro.exe`) es opcional; la consola
(`drumaspro_console.exe`) permanece como herramienta de diagnóstico. Ambas usan
el mismo `AudioEngine`, `SynthProcessor` y cola de comandos. La interfaz se
implementa en `src/ui/qt/`; los tipos musicales y el puente que expande acordes
son C++ puro y no dependen de Qt.

## Requisitos y compilación

- Windows 10/11 x64 y Visual Studio 2022 o 2026.
- Qt 6.8.3 para MSVC x64 (Core, Gui, Qml, Quick, Quick Controls 2 y Quick
  Layouts).
- `DRUMAS_BUILD_UI` es `OFF` por defecto. El motor y la consola compilan sin
  Qt; no se necesita instalar Qt para CI ni para esos targets.

Con el SDK local usado durante el desarrollo:

```powershell
cmake --preset vs2026 -DDRUMAS_BUILD_UI=ON
cmake --build --preset vs2026-release --target drumaspro_qt
```

El preset busca Qt bajo `.tools/Qt/6.8.3/msvc2022_64`. Para un SDK en otra
ubicación, establece `CMAKE_PREFIX_PATH` al directorio que contiene
`Qt6Config.cmake`. Desde la raíz del repositorio, este script configura y
compila Release, ejecuta `windeployqt` para desplegar las DLL/plugins junto al
ejecutable y abre la ventana:

```powershell
.\tools\run-ui.ps1
```

El ejecutable queda en
`build\vs2026\src\ui\qt\Release\drumaspro.exe`. Para configurar, compilar y
desplegar sin abrirlo, usa `.\tools\run-ui.ps1 -BuildOnly`.
El script selecciona el backend software de Qt Quick si no se ha definido uno
en el entorno; así usa el mismo backend con el que se producen las capturas de
verificación.

La piel predeterminada usa un panel gris-beige cálido, marcos negros y acentos
verdes discretos; el tema ámbar/oscuro sigue disponible. El volumen principal y
los parámetros tienen escalas impresas, perillas con estrías dibujadas,
selectores rotativos discretos y teclas de acorde con LED rojo. Los seis efectos
se muestran como módulos individuales con el número de slot correcto, dentro de
un marco propio con su título; el slot seleccionado lleva borde de acento y la
franja inferior es su inspector: muestra los parámetros publicados por el
registro del efecto, con rango, unidad y curva, y permite moverlos en vivo.
Todavía no hay joystick ni secuenciador.

Los controles usan un lenguaje de hardware dibujado solo con QML
(`Rectangle`, `Canvas`, gradientes), sin imágenes raster: las teclas de acorde y
los botones tienen bisel con gradiente (luz arriba-izquierda, sombra
abajo-derecha), borde interior, sombra proyectada que se reduce al pulsar y un
desplazamiento de 2 px hacia abajo; el hover añade un velo suave y el LED de
acorde enciende un halo de dos discos. Las perillas dibujan su sombra con
`shadowBlur`, un anillo de base con gradiente diagonal, estrías alternas
clara/oscura y un brillo especular radial sobre la tapa; los números de mínimo y
máximo solo se imprimen en perillas grandes (los diales compactos no tienen
espacio y muestran las marcas y el valor exacto). Los marcos principales llevan
tornillos decorativos en las esquinas, una línea de bisel clara arriba/izquierda
y oscura abajo/derecha, y el fondo usa un degradado vertical suave. Las
"mejillas" de madera laterales no se implementaron: habrían reducido el ancho
útil sin aportar legibilidad.

El `MASTER` vive en el panel SYNTHESIZER, a la derecha del grupo LFO, para no
dejar un hueco muerto en la fila de parámetros. Los selectores rotativos de
patch y escala imprimen sus posiciones alrededor del dial cuando hay como mucho
cuatro opciones (con abreviaturas de tres o cuatro caracteres); el número de
opciones y la posición activa siguen apareciendo bajo el dial. El LED de cada
slot de efecto es rojo cuando está en bypass y verde cuando procesa, además del
texto `BYPASSED`/`ACTIVE`.

Los valores se formatean por unidad en `ValueFormat.cpp`: los parámetros de
porcentaje se guardan en 0..1 pero se muestran en 0..100 (`0.55` → `55 %`), los
Hz usan 1–2 decimales solo por debajo de 100, los ms redondean a partir de 10 y
los dB/ct/oct recortan ceros sobrantes. El mismo formateador alimenta las
perillas, la franja de efectos y el LCD. El tamaño tipográfico mínimo está
centralizado en `Theme.minimumFontSize` (8 px compacto, 9 px normal); ningún
texto baja de ese tamaño.

Las fuentes son las instaladas en el sistema (Arial Narrow/Arial para
serigrafía y `monospace` para LCD); no se ha añadido ni
redistribuido una fuente. La elección de una fuente OFL verificada queda para el
pulido visual.

## Capas y flujo

- `src/music/` ofrece escalas, acordes y nombres como funciones puras.
- `src/bridge/` contiene el `CommandSink`, la expansión de acordes en un batch
  de comandos NoteOn/NoteOff y los espejos de parámetros/cadena; no conoce Qt.
- `EngineBridge` es la única fachada QObject. Su `QTimer` refresca el estado
  cada 25 ms (40 Hz), leyendo contadores atómicos y actualizando los modelos.
  El medidor muestra el pico real de la salida con decaimiento; el LCD del
  motor indica las voces activas en su rótulo y los XRUNS acumulados junto al
  estado.
- Los modelos de parámetros, patches, cadena, parámetros de efecto y acordes
  alimentan los componentes QML. Un cambio de efecto prepara el objeto en el
  hilo de control antes de transferir su propiedad por la cola; sus valores por
  defecto se copian al espejo para que el inspector muestre lo que suena.
- Un acorde se encola entero o no se encola. Si no hay capacidad para todas sus
  notas, se descarta y se cuenta; la UI no bloquea el hilo de audio. Las notas
  compartidas entre acordes mantenidos se contabilizan: liberar un acorde no
  corta una nota que otro sigue usando, y la nota común no se reataca.

El tema centraliza colores, tipografía, dimensiones y espaciado en
`Theme.qml`; el control `AMBER` alterna entre la paleta predeterminada y la
ámbar. El layout se compacta para el mínimo de 960×600 y mantiene scroll como
respaldo para restricciones de espacio del sistema. Los controles QML usan el
estilo `Basic` de Qt Quick Controls para que sus fondos y estados sean
personalizables. Cuando el foco está fuera de los controles, las teclas 1–7
mantienen y liberan los siete acordes diatónicos; el auto-repeat se ignora.
Pulsa el encabezado DRUMASPRO para devolver el foco a los atajos tras usar un
selector.

Los atajos del piano (`A`..`K`, `Z`/`X`) viven en el mismo `Keys` handler que
las teclas 1–7 y comprueban `activeFocusItem`, así que un selector o un
`RotarySelector` enfocado conserva sus flechas y no dispara notas.

## Ajustes persistentes

`Settings.cpp` envuelve `QJsonDocument` tras una interfaz propia
(`SettingsStore`): la UI nunca toca JSON. El archivo vive en
`%APPDATA%\Drumaspro\settings.json` (variable `DRUMAS_SETTINGS_PATH` para
pruebas) y lleva `schemaVersion`. La carga trata archivo ausente, vacío,
truncado o con JSON inválido como "usar defectos"; una versión futura se
rechaza entera (no se medio aplica) y los campos desconocidos se ignoran. Todos
los valores se validan: piel en {default, amber, night}, acento 0..5, escala en
{90,100,110,125}, sensibilidad 50..150, curva de velocity 0..2, octava 0..8,
tono 0..11 y pads con raíz/calidad/inversión/octava clampeados. La escritura usa
`QSaveFile` (temporal + rename) para no dejar un archivo a medias.

La escala de UI multiplica la tipografía (no la geometría de los controles): así
125 % mejora la legibilidad sin romper el encaje a 960×600. El acento es un
preset global: 0 usa el color propio de la piel y 1..5 son verde, ámbar, hielo,
violeta y óxido. El tema noche es oscuro con acento fósforo.

Cada pad puede ser `DIATONIC` (sigue la escala y las séptimas) o `MANUAL` (raíz,
calidad entre las diez de `src/music`, inversión y octava). El clamp de
`chordToMidiNotes` rechaza voicings fuera de MIDI 0..127 en vez de recortarlos.
El esquema guarda ocho slots; la UI usa siete y el octavo queda reservado. El
botón `LATCH` vive en el puente: al activarlo, soltar el pad no envía NoteOff;
otro acorde libera el anterior, y apagarlo libera el que estuviera sostenido. Los
LED de los pads siguen el estado real del latch.

El diálogo `SETUP` aplica los cambios en vivo (bindings sobre el puente) y
muestra la ruta del archivo. El selector de dispositivo de audio y el tamaño de
buffer aparecen como pendientes: el motor no expone todavía una API segura para
cambiarlos en caliente, así que no se implementaron a medias.

## Piano en pantalla y teclado del ordenador

La franja de acordes y el piano comparten sitio: el botón `KEYS` del panel
SYNTHESIZER conmuta entre `CHORDS` y `KEYS`. Fue la única forma de que ambos
caben a 1280×720 y 960×600 sin scroll; con la vista de piano abierta las teclas
1–7 siguen tocando acordes y sus notas se resaltan sobre el teclado.

`PianoKeyboard.qml` dibuja dos octavas desde C3 a 960×600 y tres a partir de
1100 px de ancho de ventana (21 teclas blancas). Las teclas blancas y negras usan
el mismo lenguaje de bisel y gradiente que el resto del panel, se hunden 2 px al
pulsarlas y se pintan con el color de acento cuando suenan. El clic y el arrastre
hacen glissando (se libera la nota anterior antes de atacar la siguiente) y la
velocity se calcula por la altura vertical del clic (0.45 arriba, 1.0 abajo).
Las teclas `C` llevan serigrafía de octava.

El teclado del ordenador mapea `A W S E D F T G Y H U J K` a una octava
cromática desde C, y `Z`/`X` bajan/suben la octava (0..8, con las notas
limitadas a MIDI 0..127). El auto-repeat se ignora, las teclas no se capturan si
el foco está en un control, y al perder el foco se liberan todas las notas
mantenidas. El LCD `OCTAVE` muestra la octava actual (`C4`) y el recordatorio
`Z / X`. Las notas del piano y de los acordes comparten los contadores de
referencia del puente: una nota sostenida por un acorde y por el teclado solo se
libera cuando ya no la necesita ninguno.

## Rendimiento

La UI no tiene bucle de animación: repinta cuando cambia un valor o el tema. La
prueba `fullFrameRenderFitsSixtyFps` mide un repintado completo a 1280×720 con
el backend software offscreen: **3,2 ms por frame** (≈309 fps de margen, frente
alos 16,6 ms de 60 fps) en la máquina de desarrollo. Por eso no se añadieron
`layer.enabled`: las decoraciones son rectángulos baratos y el contenido de los
`Canvas` queda retenido hasta que se pide `requestPaint()`; las capas habrían
añadido memoria y una pasada extra sin necesidad. Si el coste sube al añadir
más controles, la primera palanca es cachear los fondos de panel.

## Pruebas y captura

Con `DRUMAS_BUILD_UI=ON`, `ctest --preset vs2026-release` incluye `qt_ui_tests`.
Ese test carga la pantalla en la plataforma `offscreen`, comprueba los modelos
(incluido el registro de parámetros de cada efecto y la selección de slot) y
compara los batches enviados por botones y atajos; también guarda capturas a
1280×720 y 960×600 en ambos temas para inspección. La prueba `qml_theme_singleton`
inspecciona el `qmldir` que CMake genera para la aplicación y falla si `Theme`
deja de registrarse como singleton. No se abre un dispositivo de audio. Las imágenes son evidencia de render QML y no certifican el aspecto en
una ventana nativa ni la calidad sonora. La revisión manual de audio/dispositivo sigue pendiente; véase
[`ui-checklist.md`](./ui-checklist.md).

La aplicación de consola y la prueba de aislamiento se compilan sin Qt. La CI
configura explícitamente `DRUMAS_BUILD_UI=OFF`; la compilación local con Qt
activa cubre adicionalmente la pantalla y sus modelos.
