# Vendored third-party sources

Everything in this directory is checked in as source, at a fixed version. No
branch, no `latest`, no submodule, no download at configure time. The versions
below were read from the upstream repositories themselves.

| Directory | Upstream | Version | Commit | License |
|---|---|---|---|---|
| `miniaudio/` | <https://github.com/mackron/miniaudio> | `0.11.26` (header dated 2026-10-31) | tag `0.11.26` | Public domain / MIT-0, at our choice |
| `dspark/` | <https://github.com/CristianMoresi/DSPark> | `1.7.0` (tag `v1.7.0`, released 2026-08-21) | `e5e13a2632abadb17272c5c6413877c3295ef446` | MIT |

License texts: see [`../THIRD_PARTY_LICENSES.md`](../THIRD_PARTY_LICENSES.md).

## miniaudio

Files taken verbatim from the tag:

| File | SHA-256 |
|---|---|
| `miniaudio/miniaudio.h` | `5b325f7d1a6fd72f94294963a441e2442887bc43f63df796a12c4a75f38b22c6` |
| `miniaudio/miniaudio.c` | `c604ede9d3b35873f8bf785213e899ad21d9658043c31465421261c2413a7f65` |
| `miniaudio/LICENSE` | upstream `LICENSE` |

`miniaudio.c` is the implementation translation unit (`#define
MINIAUDIO_IMPLEMENTATION`, then `#include "miniaudio.h"`). It is compiled as C,
once, into the static library `drumas::miniaudio`. Unused subsystems are switched
off with `MA_NO_DECODING`, `MA_NO_ENCODING`, `MA_NO_RESOURCE_MANAGER`,
`MA_NO_NODE_GRAPH`, `MA_NO_ENGINE` and `MA_NO_GENERATION`; only the device layer
(and the WASAPI backend it selects on Windows) is built.

## DSPark

Only the `Core/` headers are vendored (41 headers): `SpscQueue.h`,
`SmoothedValue.h`, `Oscillator.h`, `EnvelopeGenerator.h`, `DenormalGuard.h`,
`DspMath.h` and what they include. The rest of the upstream tree is left out on
purpose:

- `Effects/`, `Analysis/`, `Music/` and `IO/` are not used in this stage.
- `DSParkLab/` and `plugin/` bundle their own third-party code (Dear ImGui,
  miniaudio, VST3/CLAP SDK headers) under different licenses; they must never
  enter this repository.

To add another upstream header, take it from the same tag
(`v1.7.0`, commit `e5e13a2`) so the vendored tree stays a consistent subset, and
update this file.

### Threading contract used

`docs/threading.md` of the upstream tag defines the contract this project relies
on, and it is what `src/core/SpscQueue.h` wraps:

- exactly one producer thread calls `push()`, exactly one consumer calls `pop()`;
- the queue is wait-free, lock-free and allocation-free in both;
- `Capacity` must be a power of two and the usable capacity is `Capacity - 1`
  (one slot is reserved to tell full from empty).

`SmoothedValue` and `Oscillator` are documented as **not** thread-safe: they are
only touched by the audio thread, after `prepare()` on the setup thread.
`DenormalGuard` sets FTZ/DAZ on the calling thread and is constructed inside the
audio callback.

## Updating a dependency

1. Pick an upstream tag, never a branch.
2. Replace the files, keeping only the subset described above.
3. Record the tag, the commit hash and the new SHA-256 hashes here.
4. Update `../THIRD_PARTY_LICENSES.md` if the license text changed.
5. Re-run the full build and test suite (`ctest --preset vs2026-debug`) before
   committing.
