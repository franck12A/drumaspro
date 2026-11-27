# ADR 0015 — Acordes como grupos de voces y política de robo

**Estado:** aceptado (Etapa 5B, bloque 1).
**Sustituye en parte:** la expansión de acordes en notas sueltas descrita en
`docs/rt-safety.md` y `docs/ui.md`.

## Contexto

Hasta ahora el puente expandía un acorde en un batch de `NoteOn`/`NoteOff` y
llevaba sus propias cuentas de referencias por nota para que soltar un acorde no
cortase una nota compartida con otro. Eso funcionaba, pero:

- la propiedad del acorde vivía en la UI, no en el motor, así que cualquier otro
  cliente (consola, futuros controladores) tenía que reimplementarla;
- `VoicePool::noteOn` reutilizaba la voz que ya sonaba esa nota, de modo que un
  acorde podía *retriggerar* una nota que otro acorde mantenía;
- la voz robada se reiniciaba en el acto, con riesgo de escalón audible.

## Decisión

1. **Un acorde es un comando.** `CommandType::ChordOn` lleva hasta cuatro notas
   empaquetadas (`notes`, un byte por nota), la velocity en `value` y un `group`
   de 1 a 31. `ChordOff` lleva solo el `group`. Ambos siguen siendo
   trivialmente copiables y viajan por la misma cola SPSC.
2. **Asignación atómica.** `VoicePool::chordOn` elige todas las voces antes de
   arrancar ninguna: un acorde nunca suena a medias y todas sus notas empiezan
   en la primera muestra del bloque. Un `ChordOn` inválido (grupo 0, más de
   cuatro notas) se rechaza entero.
3. **Máscara de grupos por voz.** Cada voz guarda un `uint32_t` con un bit por
   grupo. Si una nota ya está mantenida por otro acorde, el nuevo acorde se une
   a esa voz (`addGroup`) en lugar de reatacarla; `chordOff` limpia solo su bit
   y la voz solo entra en release cuando la máscara queda a cero. Las notas
   comunes entre acordes no se reatacan ni se cortan.
4. **Notas sueltas separadas.** El piano y la consola usan `NoteOn`/`NoteOff`
   sin grupo. Una nota suelta nunca reutiliza ni libera una voz de acorde: si
   coincide con una nota mantenida por un pad, ocupa otra voz. Así el teclado no
   puede robar ni apagar un acorde.
5. **Robo.** Con el pool lleno se elige primero una voz **en release** (la más
   antigua); si no hay ninguna, la más silenciosa, y a igualdad la más antigua.
   La voz reutilizada arranca con un *fade-in* de 2 ms (`SynthVoice::kFadeInMs`)
   para que el ataque no dé un escalón. Las voces del acorde anterior siguen su
   release normal (el del patch), de modo que el cambio de acorde se solapa en
   lugar de cortar.
6. **El puente ya no expande.** `bridge::ChordExpander` emite un solo comando y
   `EngineBridge` conserva únicamente el estado de "qué pad está pulsado" y el
   latch. `noteRefCounts_` queda solo para pulsaciones repetidas del piano.

## Consecuencias

- El motor es el único dueño de la polifonía y de las notas compartidas; los
  clientes no necesitan llevar cuentas.
- Un `ChordOff` con el grupo equivocado ya no puede silenciar el acorde que el
  usuario tiene pulsado (el bug anterior venía de resolver el acorde por fila).
- El límite de cuatro notas es explícito (`core::kMaxChordNotes`): un voicing
  mayor se rechaza, no se trunca.
- Coste: 32 bits por voz y un `uint32_t` más en `Command`; sin asignaciones ni
  ramas adicionales en el render (la máscara solo se toca en note-on/off).
- Tests: `tests/synth/test_voice_pool.cpp` (arranque atómico, notas compartidas,
  ChordOff por grupo, robo finito) y
  `changing chord with shared notes stays continuous` en
  `tests/synth/test_patch_switching.cpp` (sin discontinuidad audible).
