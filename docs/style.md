# Guía de estilo

## Herramientas

- `.clang-format` en la raíz: base LLVM, 4 espacios, 100 columnas, llaves
  adjuntas, punteros a la izquierda. Formatea siempre antes de commitear:

  ```powershell
  clang-format -i src/**/*.h src/**/*.cpp tests/**/*.cpp
  ```

- `.clang-tidy` en la raíz: `bugprone-*`, `performance-*`, `modernize-*` y los
  `cppcoreguidelines-*` relevantes. No bloquea el build; se activa con
  `cmake --preset vs2026-tidy` (o `vs2022-tidy`).
- `.editorconfig`: UTF-8, LF, 4 espacios (2 en YAML/JSON). `third_party/` queda
  fuera de todo esto: se mantiene byte a byte igual al upstream.

## Nombres

| Elemento | Convención | Ejemplo |
|---|---|---|
| Tipos, clases, enums | `PascalCase` | `AudioGraph`, `CommandType` |
| Funciones y métodos | `camelCase` | `pushCommand`, `noteToFrequency` |
| Variables y parámetros | `camelCase` | `numFrames`, `sampleRate` |
| Miembros privados | `camelCase_` | `planeStorage_`, `deviceEvents_` |
| Constantes | `kPascalCase` | `kDefaultSampleRate`, `kMaxVoices` |
| Namespaces | `lower_case` | `drumas::core`, `drumas::engine` |
| Ficheros | `PascalCase.h/.cpp` | `AudioEngine.cpp` |

## Reglas

- Código C++20, sin extensiones del compilador (`/permissive-`).
- `[[nodiscard]]` en todo lo que devuelve un resultado que no debe ignorarse.
- Sin `using namespace` en cabeceras; en `.cpp` de tests, nunca.
- Cabeceras con `#pragma once`.
- Cada función del camino de audio indica en su comentario si es **RT-SAFE** o
  solo de setup. Los contratos de hilos se documentan en la cabecera, no se
  sobreentienden.
- Los comentarios de código están en inglés; la documentación de producto y los
  ADR, en español. Así el código es legible para cualquiera que use la librería
  y las decisiones quedan en el idioma del proyecto.
- Nada de `#define` para constantes: `inline constexpr` o `static constexpr`.
- Errores por valor de retorno (`bool` + `lastError()`), no por excepción, en
  todo lo que toque audio o dispositivo.

## Cabeceras third-party

Los headers de `third_party/` se incluyen con su prefijo (`<dspark/Core/...>`,
`"miniaudio.h"`). El prefijo no es cosmético: en Windows, sin él,
`<Core/SpscQueue.h>` resuelve a nuestro propio `src/core/SpscQueue.h` porque el
sistema de ficheros ignora mayúsculas.
