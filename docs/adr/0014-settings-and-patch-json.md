# 0014 — Settings y patches en JSON (QJsonDocument), sin dependencia nueva

## Contexto

Necesitábamos persistir ajustes de UI, mapeos de pads y patches de usuario sin
añadir una dependencia de terceros, sin tocar el hilo de audio y sin romper el
aislamiento del motor (core/music/synth/fx/engine/bridge no enlazan Qt).

## Decisión

- **Formato**: JSON con `schemaVersion` en ambos archivos.
- **Librería**: `QJsonDocument`/`QSaveFile` de Qt Core, ya presente por la UI
  (LGPL dinámico). No se vendoriza nlohmann/json: sería una dependencia extra
  para capacidades que Qt ya cubre.
- **Ubicación**: `%APPDATA%\Drumaspro\settings.json` y
  `%APPDATA%\Drumaspro\patches\*.json`. `DRUMAS_SETTINGS_PATH` y
  `DRUMAS_PATCHES_PATH` permiten usar rutas temporales en tests.
- **Escritura atómica**: archivo temporal + rename mediante `QSaveFile`; un
  corte a mitad no deja JSON truncado como archivo bueno.
- **Lectura tolerante**: archivo ausente/vacío/truncado/inválido → valores por
  defecto (nunca crash). Campos desconocidos → ignorados. Valores fuera de
  rango → clamp contra el `ParameterRegistry` (la UI no duplica rangos).
  `schemaVersion` futura → rechazo completo con error legible.
- **Patches**: las claves del objeto `params` son los **nombres estables** del
  registro (`filter.cutoff`, `amp.attack`...), no hashes; al cargar se mapean a
  los IDs y a los campos de `Patch`. Los patches de usuario llevan `name` y
  `category` fuera de la estructura trivialmente copiable del motor.
- **Aplicación al motor**: `SelectPatchData` transfiere un `Patch*` por la cola
  SPSC; el hilo de audio copia el valor y devuelve el puntero por una cola de
  retirada. El hilo de control lo libera en su sondeo de 40 Hz. No hay
  asignaciones ni borrados en el callback.
- **Pads**: el mapeo manual/diatónico se guarda en settings, no en el motor; el
  bridge lo expande a notas MIDI antes de encolar.

## Consecuencias

- El motor sigue sin Qt ni red; los tests de aislamiento lo verifican.
- Los archivos son inspeccionables y editables a mano; un valor absurdo se
  clampa en vez de producir NaN.
- `Patch::name` sigue siendo una vista a un literal de fábrica: los nombres de
  usuario viven en la capa UI. Un patch de usuario no aparece en el banco de
  fábrica, sino como patch cargado (`USER`).
- No hay migración de versiones anteriores todavía: solo `schemaVersion` 1. Una
  versión futura se rechaza en lugar de interpretarse mal.
