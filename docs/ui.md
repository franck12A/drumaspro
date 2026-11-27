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
se muestran como módulos individuales con el número de slot correcto; el slot
seleccionado lleva borde de acento y la franja inferior es su inspector: muestra
los parámetros publicados por el registro del efecto, con rango, unidad y curva,
y permite moverlos en vivo. Todavía no hay joystick ni secuenciador.

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
