# AddressSanitizer con MSVC

MSVC soporta ASan en x64 desde Visual Studio 2019 16.9. Se ha verificado que el
toolset instalado (`v145`, VS 2026) acepta `/fsanitize=address`; la ejecución de
la suite bajo ASan queda documentada aquí para quien la necesite.

## Configurar un árbol dedicado

ASan necesita CRT dinámico (el valor por defecto, `/MD`) y enlazado incremental
desactivado. Se configura un árbol aparte para no ensuciar los presets normales:

```powershell
cmake -S . -B build/asan -G "Visual Studio 18 2026" -A x64 `
      -DCMAKE_CXX_FLAGS="/fsanitize=address" `
      -DCMAKE_EXE_LINKER_FLAGS="/INCREMENTAL:NO"
cmake --build build/asan --config Debug
ctest --test-dir build/asan -C Debug --output-on-failure
```

Con VS 2022, sustituye el generador por `"Visual Studio 17 2022"`.

## Si el ejecutable no arranca

El binario de ASan necesita su runtime (`clang_rt.asan_dbg_dynamic-x86_64.dll`
en Debug, `..._dynamic-x86_64.dll` en Release). Desde una consola de
*Developer PowerShell for VS* se encuentra solo; si no, añade al `PATH` el
directorio `VC\Tools\MSVC\<versión>\bin\Hostx64\x64` o el que contenga esa DLL.

## Qué cubre y qué no

- Detecta desbordamientos de heap y de stack, uso tras liberar y fugas (con
  `ASAN_OPTIONS=detect_leaks=1`).
- **No** detecta carreras de datos: para eso hace falta ThreadSanitizer, que MSVC
  no ofrece. La concurrencia se cubre con el test de dos hilos de la cola SPSC,
  los atómicos del grafo y la revisión de código descrita en
  [`rt-safety.md`](rt-safety.md).
- El hilo de audio real no se ejecuta bajo ASan en esta etapa; los tests cubren
  el mismo `render()` de forma offline.
