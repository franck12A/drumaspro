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
- `core::ScopedDenormalGuard`, que activa FTZ/DAZ en el hilo de audio.

## Contratos por clase

| Clase | No real-time-safe | Real-time-safe |
|---|---|---|
| `core::SpscQueue` | construcción | `push` (productor), `pop` (consumidor) |
| `core::SmoothedValue` | `prepare` (setup) | `setTargetValue`, `getNextValue` |
| `core::ScopedDenormalGuard` | — | constructor/destructor |
| `AudioGraph` | `prepare` | `pushCommand`, `render`, `stats` |
| `SynthProcessor` | `prepare`, `reset` | `process`, `applyCommand` |
| `SineVoice` / `VoicePool` | `prepare` | `noteOn`, `noteOff`, `renderAdd` |
| `SineOscillator` / `AdsrEnvelope` | `prepare` | `nextSample` / `nextValue` |

`render()` es el único punto de entrada del hilo de audio: drena la cola, llama
al procesador y actualiza los contadores atómicos. Todo lo demás (crear la voz,
cambiar un parámetro) llega como `core::Command` por la cola SPSC.

## Comunicación entre hilos

- **Control → audio**: `core::SpscQueue<core::Command, 512>`. Un productor, un
  consumidor, sin locks ni asignaciones. Si la cola se llena, el evento se
  descarta y se cuenta (`commandsDropped`); nunca se bloquea al hilo de UI.
- **Audio → control**: `std::atomic` por valor. Los contadores de bloque,
  frames, glitches, callbacks pasados de tiempo, tiempo máximo y número de voces
  activas son palabras atómicas relajadas: el hilo de audio nunca se bloquea y la
  UI puede leer un valor ligeramente obsoleto, que es lo correcto para un medidor.
- **Audio → audio**: los procesadores no son thread-safe a propósito (sin
  barreras en el camino caliente). Los setters se llaman solo desde el hilo de
  audio, o desde el hilo de control con el stream parado.

## Suavizado de parámetros

Todo parámetro continuo que llega a la DSP pasa por `core::SmoothedValue`
(rampa lineal, 10 ms por defecto). Una ganancia que salta produce un clic; una
rampa no. Los cambios de nota no usan rampa de frecuencia porque el oscilador
conserva la fase: un seno con fase constante no tiene discontinuidad de valor.

## Protección contra denormales

`core::ScopedDenormalGuard` se construye al principio del callback y activa
FTZ/DAZ en ese hilo. Sin él, una cola de release decayendo hacia cero entra en
números subnormales y multiplica el coste por muestra.

## Cómo se verifica

1. **Test automático de cero asignaciones.**
   `tests/support/AllocationCounter.cpp` reemplaza los `operator new/delete`
   globales del ejecutable de tests y los cuenta. El test
   «rendering a block on the audio path does not allocate» prepara todo, hace un
   bloque de calentamiento, reinicia el contador y renderiza 128 bloques con el
   `SynthProcessor` real: el contador debe quedarse en cero. `render()` es
   literalmente la función que llama el callback del dispositivo.
2. **Revisión de código del callback.** El callback
   (`AudioEngine::dataCallback`) tiene tres sentencias: obtener el `AudioEngine*`,
   construir el guard de denormales y llamar a `graph_.render(...)`. No hay
   `new`, `mutex`, `printf` ni excepciones; `AudioGraph::render` solo usa
   `std::fill_n`, aritmética, la cola SPSC y atómicos.
3. **Revisión de contenedores.** `std::vector` aparece solo en `prepare()`
   (`AudioGraph::planeStorage_`, `SynthProcessor::mix_`); tras `prepare()` no se
   redimensiona nada.
4. **Ejecución real.** La app de consola muestra glitches de stream, callbacks
   pasados de tiempo y tiempo máximo de callback mientras suena.
