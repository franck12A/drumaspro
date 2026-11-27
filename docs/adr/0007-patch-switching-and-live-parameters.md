# ADR 0007 — Cambio de patch, parámetros en vivo y LFO global

- **Estado:** aceptado (Etapa 4)
- **Fecha:** 2026-11-24

## Contexto

Una sola arquitectura de voz parametrizable debe servir para pad, pluck y
teclas. Los patches se cambian en caliente con un acorde sonando y el usuario
puede mover cutoff, resonancia y release sin que la señal chasquee. Además hay
que decidir si el LFO es por voz o global.

## Decisiones

### 1. Un patch es datos; la voz lo captura al hacer note-on

`synth::Patch` es un struct trivialmente copiable (osciladores, unison, filtro,
envolventes, velocity y ruteo de LFO). Añadir un instrumento es añadir una
entrada a `PatchBank.h`, no una clase.

Al recibir `NoteOn`, la voz copia el patch vigente. `SelectPatch` cambia el
patch que usarán las **notas nuevas** y no toca las voces que ya suenan: cada
una termina con el patch con el que nació. Es la política más simple que
garantiza cero clics, porque no hay ningún parámetro de la voz que cambie en
medio de una nota.

No se implementa crossfade entre patches: exigiría duplicar voces (o reservar
un segundo pool) y el cambio de timbre se percibiría igualmente; la cola de
release con el patch viejo es musicalmente correcta y cuesta cero CPU.

### 2. Parámetros en vivo por encima del patch capturado

El processor mantiene `LiveParameters` con los valores que un `SetParam` puede
mover sobre voces ya sonando:

- `filter.cutoff` y `filter.resonance`, con rampa de 10 ms por voz;
- `osc1.level` y `osc2.level`, con una rampa independiente por voz;
- los tiempos ADSR de amplitud y de filtro, recalculando las pendientes de la
  envolvente (la etapa actual continúa desde el nivel que tenía);
- `master.gain`, como hasta ahora, suavizado.

`SetParam` actualiza el valor en vivo y lo aplica a **todas** las voces. Una
voz nueva arranca directamente en el valor en vivo (su envolvente empieza en
cero, así que no hay rampa que oír).

`SelectPatch` reinicia los valores en vivo a los del patch nuevo, pero no se
los aplica a las voces que ya suenan: siguen con su patch capturado. El
siguiente note-on ya usa los valores nuevos.

Los parámetros de timbre que no están en el conjunto en vivo (forma de onda,
unison, profundidad de LFO) solo cambian al cambiar de patch o al tocar una nota
nueva. Se documenta como limitación de la etapa, no como comportamiento
accidental.

### 3. LFO global

Un único LFO recorre una muestra por bloque y todas las voces leen el mismo
valor. Motivos:

- **Coste determinista:** un oscilador por muestra para todo el pool en vez de
  32. En el peor caso (32 voces), el LFO por voz multiplicaría por 32 un coste
  que no aporta nada audible en un pad o un órgano.
- **Coherencia musical:** el vibrato y el barrido de cutoff están en fase en
  todo el acorde, que es lo que se espera de un instrumento y no de 32
  osciladores independientes.
- **Formas básicas sin alias:** a 0,05-20 Hz un cuadrado o una sierra ingenua
  no generan alias audible; los saltos de cutoff de un LFO cuadrado los absorbe
  la rampa de cutoff del filtro.

El precio es que no hay *drift* analógico por voz. Si un patch lo pide, se
puede añadir un desfase de fase por voz sin cambiar esta arquitectura.

### 4. `Command` sigue siendo trivialmente copiable

`SelectPatch` usa un byte `patch` en el propio comando; `SetParam` usa el id de
32 bits del registro. El `static_assert` de `Command` sigue en pie: el comando
viaja por la cola SPSC por valor, sin punteros ni asignaciones.

## Consecuencias

- Si una voz es robada después de un cambio de patch, adopta el patch nuevo en
  mitad de la nota. El robo elige la voz más silenciosa (envolvente baja), así
  que el cambio es pequeño; un crossfade real queda para una etapa posterior.
- Las automatizaciones de parámetros que no son en vivo necesitan un note-on
  nuevo o un cambio de patch. La UI de la Etapa 8 puede ampliar el conjunto en
  vivo sin tocar la cola ni el contrato de hilos.
- El LFO global hace que un cambio de `lfo.rate` afecte a todas las voces al
  instante; es lo deseado para un control de "modulación" del instrumento.

## Enmienda (2026-11-26)

Se añadieron `osc1.level` y `osc2.level` al conjunto en vivo. Cada voz aplica su
nivel con una rampa de 10 ms, y `SelectPatch` sigue sin tocar las voces que ya
suenan: los niveles nuevos solo llegan a las notas siguientes salvo que el
usuario mueva el control, que es intencionalmente global. El resto del ADR
sigue vigente.
