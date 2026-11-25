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

La UI usa fuentes instaladas en el sistema (Segoe UI para etiquetas y una fuente
monoespaciada del sistema para las pantallas LCD); no incluye fuentes
redistribuidas.

## Capas y flujo

- `src/music/` ofrece escalas, acordes y nombres como funciones puras.
- `src/bridge/` contiene el `CommandSink`, la expansión de acordes en un batch
  de comandos NoteOn/NoteOff y los espejos de parámetros/cadena; no conoce Qt.
- `EngineBridge` es la única fachada QObject. Su `QTimer` refresca el estado
  cada 25 ms (40 Hz), leyendo contadores atómicos y actualizando los modelos.
- Los modelos de parámetros, patches, cadena y acordes alimentan los
  componentes QML. Un cambio de efecto prepara el objeto en el hilo de control
  antes de transferir su propiedad por la cola.
- Un acorde se encola entero o no se encola. Si no hay capacidad para todas sus
  notas, se descarta y se cuenta; la UI no bloquea el hilo de audio.

El tema centraliza colores, tipografía, dimensiones y espaciado en
`Theme.qml`; `ThemeAmber.qml` aplica el tema de acento. Los controles QML usan
el estilo `Basic` de Qt Quick Controls para que sus fondos y estados sean
personalizables.

## Pruebas y captura

Con `DRUMAS_BUILD_UI=ON`, `ctest --preset vs2026-release` incluye `qt_ui_tests`.
Ese test carga la pantalla en la plataforma `offscreen`, comprueba los modelos,
verifica que bloquear el hilo de pruebas no detiene un `AudioGraph` procesando
y guarda `docs/screenshots/drumaspro-ui.png`. No abre un dispositivo de audio.
La imagen es evidencia de render QML y no certifica el aspecto en una ventana
nativa ni la calidad sonora. La revisión manual sigue pendiente; véase
[`ui-checklist.md`](./ui-checklist.md).

La aplicación de consola y la prueba de aislamiento se compilan sin Qt. La CI
configura explícitamente `DRUMAS_BUILD_UI=OFF`; la compilación local con Qt
activa cubre adicionalmente la pantalla y sus modelos.
