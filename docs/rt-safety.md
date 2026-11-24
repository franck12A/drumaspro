# Reglas del hilo de audio

El callback de audio tiene un presupuesto de tiempo fijo (un bloque) y ninguna
forma de recuperarse: si se pasa, se oye un chasquido. Estas son las reglas que
el código cumple y cómo se comprueba.

## Prohibido dentro del callback

- `new` / `delete` / `malloc` / `free` / contenedores que crezcan.
- Mutex, `std::lock_guard`, `std::condition_variable`, spinlocks (aunque sean sin
  contención garantizada).
- E/S: ficheros, consola, `printf`, logs, red.
- Excepciones: ningún camino del callback lanza ni captura.
- Llamadas bloqueantes al sistema (esperar a otro hilo, `Sleep`, `WaitForSingleObject`).
- Compartir estado mutable sin sincronizar con otro hilo.

## Permitido y esperado

- Aritmética de coma flotante, ramas, `std::span`, `std::array` de tamaño fijo.
- `std::atomic` con órdenes `relaxed` / `release` / `acquire`.
- `std::chrono::steady_clock::now()` (una lectura de contador, no bloquea).
- Lectura/escritura de memoria preasignada en `prepare()`.
- `core::DenormalGuard`, que activa FTZ/DAZ en el hilo de audio.

## Contratos por clase

| Clase | No real-time-safe | Real-time-safe |
|---|---|---|
| `core::SpscQueue` | construcción | `push` (productor), `pop` (consumidor) |
| `core::SmoothedValue` | `prepare` (setup) | `setTargetValue`, `getNextValue` |
| `core::DenormalGuard` | — | constructor/destructor |
| `core::ParameterRegistry` | `add`, `clear` | `find`, `toReal`, `toNormalized`, `clampToRange` |
| `AudioGraph` | `prepare` | `pushCommand`, `render`, `stats` |
| `SynthProcessor` | `prepare`, `reset` | `process`, `applyCommand`, `latencySamples` |
| `fx::EffectChain` | `prepare`, constructor/destructor | `process`, `reset`, `swapEffect`, `setBypass`, `setMix`, `setParameter`, `takeRetired` |
| `fx::IEffect` | `prepare` | `process`, `reset`, `setParameter`, `latencySamples`, `tailSamples` |
| `fx::MasterLimiter` | `prepare` | `process`, `reset`, `latencySamples` |
| `VoicePool` | `prepare` | `noteOn`, `noteOff`, `renderAdd`, `setLiveParameters` |
| `SynthVoice` | `prepare`, `reset` | `noteOn`, `noteOff`, `setLiveParameters`, `renderAdd` |
| `BandLimitedOscillator` | `prepare` | `setFrequency`, `setWaveform`, `setPulseWidth`, `nextSample` |
| `ResonantLowPassFilter` | `prepare`, `reset` | `setCutoff`, `setResonance`, `processStereo` |
| `Lfo` | `prepare` | `setRate`, `setShape`, `nextValue` |
| `AdsrEnvelope` | `prepare` | `nextValue` |
| `fx::MasterLimiter` / `fx::SafetyClip` | `prepare` / — | `process`, `reset`, `latencySamples` / `processSample` |

`render()` es el único punto de entrada del hilo de audio: drena la cola, llama
al procesador y actualiza los contadores atómicos. Todo lo demás (crear la voz,
cambiar un parámetro) llega como `core::Command` por la cola SPSC.

## Comunicación entre hilos

- **Control → audio**: `core::SpscQueue<core::Command, 512>`. Un productor, un
  consumidor, sin locks ni asignaciones. Si la cola se llena, el evento se
  descarta y se cuenta (`commandsDropped`); nunca se bloquea al hilo de UI.
- **Audio → control**: `std::atomic` por valor. Los contadores de bloque,
  frames, glitches, callbacks pasados de tiempo, tiempo máximo, número de voces
  activas y latencia total son palabras atómicas relajadas: el hilo de audio
  nunca se bloquea y la UI puede leer un valor ligeramente obsoleto, que es lo
  correcto para un medidor. Además, el audio devuelve los efectos retirados por
  `core::SpscQueue<fx::IEffect*, 16>`; el control es el único que los destruye.
- **Audio → audio**: los procesadores no son thread-safe a propósito (sin
  barreras en el camino caliente). Los setters se llaman solo desde el hilo de
  audio, o desde el hilo de control con el stream parado.

## Suavizado de parámetros

Todo parámetro continuo que llega a la DSP pasa por `core::SmoothedValue`. La
mezcla y el bypass usan rampas lineales de 10/20 ms; los parámetros medidos en
Hz, ms o muestras (cutoff, tone, tiempo de delay, compensación de drive) usan
suavizado exponencial, porque la rampa lineal de DSPark está normalizada a 0..1
y no sirve para un rango de 20..20000. Una ganancia que salta produce un clic;
una rampa no. Los cambios de nota no usan rampa de frecuencia porque el
oscilador conserva la fase: una frecuencia distinta con fase continua no tiene
discontinuidad de valor. Un cambio de patch no toca las voces que suenan: cada
una conserva el patch que capturó en su note-on (ADR 0007).

## Protección contra denormales

`core::DenormalGuard` se construye al principio del callback y en los efectos
propios (filtro, delay y drive), y activa FTZ/DAZ en ese hilo. Sin él, una cola
de release o un feedback decayendo hacia cero entra en números subnormales y
multiplica el coste por muestra.

## Cómo se verifica

1. **Test automático de cero asignaciones.**
   `tests/support/AllocationCounter.cpp` reemplaza los `operator new/delete`
   globales del ejecutable de tests y los cuenta. Los tests
   «rendering a block on the audio path does not allocate» y «six inserts and the
   look-ahead limiter run without allocating» preparan todo, cargan el patch de
   mayor coste (Pad, unison 4), tocan 32 voces, activan los seis slots, hacen un
   bloque de calentamiento, reinician el contador y renderizan con el
   `SynthProcessor` real: el contador debe quedarse en cero. El test de swap
   comprueba además que reemplazar un efecto en caliente no asigna. `render()` es
   literalmente la función que llama el callback del dispositivo.
2. **Revisión de código del callback.** El callback
   (`AudioEngine::dataCallback`) tiene tres sentencias: obtener el `AudioEngine*`,
   construir el guard de denormales y llamar a `graph_.render(...)`. No hay
   `new`, `mutex`, `printf` ni excepciones; `AudioGraph::render` solo usa
   `std::fill_n`, aritmética, la cola SPSC y atómicos.
3. **Revisión de contenedores.** `std::vector` aparece solo en `prepare()`
   (`AudioGraph::planeStorage_`, `SynthProcessor::mixLeft_/mixRight_/lfoBuffer_`,
   los buffers secos/húmedos de `EffectChain` y los delay lines de los efectos);
   tras `prepare()` no se redimensiona nada. La voz, el pool, el LFO, el
   registro de parámetros, la cola de comandos y la de retiro son de tamaño
   fijo.
4. **Ejecución real.** La app de consola muestra glitches de stream, callbacks
   pasados de tiempo y tiempo máximo de callback mientras suena.
