# Checklist de escucha (pendiente de verificación humana)

Ninguna de estas comprobaciones se puede hacer con tests: requieren oído. El
código y las mediciones offline están verificados; esta lista es lo que queda
por confirmar en una sesión de escucha real.

## Preparación

- Ejecutar `drumaspro_console.exe --buffer=256 --rate=48000` con el volumen del sistema
  a un nivel moderado.
- Tocar acordes de cuatro notas mantenidos (por ejemplo `A` + `D` + `G` + `J`)
  y dejar que la cola de release termine.
- Repetir con `F1` (Pad), `F2` (Pluck) y `F3` (Keys).

## General

- [ ] No hay chasquido al atacar ni al soltar ninguna nota (la consola suelta
      sola a los 400 ms).
- [ ] Subir/bajar cutoff con `1` + flechas mientras suena un acorde no produce
      zumbidos ni pasos: el barrido es continuo.
- [ ] Subir/bajar resonancia con `2` no silba de forma descontrolada ni se
      desborda.
- [ ] Subir/bajar release con `3` y soltar notas: la cola cambia sin clic.
- [ ] Cambiar de patch (`F1`/`F2`/`F3`) con un acorde sonando: las notas
      antiguas conservan su timbre y no hay clic; la siguiente nota usa el
      patch nuevo.
- [ ] Ningún acorde de cuatro notas suena duro ni distorsionado; si el limitador
      actúa, se oye como compresión suave, no como recorte.

## Pad (`F1`)

- [ ] Ataque lento y audible (alrededor de 0,7 s), sin bombeo ni chasquido.
- [ ] Release largo y suave; el acorde se apaga sin cortarse.
- [ ] El unison se oye ancho y desafinado, sin zumbido metálico ni cancelación
      en mono.
- [ ] El filtro es oscuro pero no embarrado; al subir cutoff (`1`) aparece
      brillo de forma progresiva.
- [ ] El vibrato es muy leve y no desafina el acorde.

## Pluck (`F2`)

- [ ] El ataque es inmediato y percusivo, sin clic de envolvente.
- [ ] El decay es corto y la nota cae a un sustain bajo; se distingue el
      "pluck" de la envolvente de filtro.
- [ ] Al tocar notas altas no se oyen pájaros ni aliasing metálico.
- [ ] En acorde, las cuatro notas se distinguen sin barro ni saturación.

## Keys (`F3`)

- [ ] Timbre de piano eléctrico/órgano ligero, con cuerpo y sin estridencia.
- [ ] El decay medio deja una cola natural; el sustain no queda demasiado alto.
- [ ] Al subir la velocity (requiere UI o test; la consola envía siempre 1.0) el
      volumen y sobre todo el brillo cambian de forma clara.
- [ ] En acorde, el registro medio no se satura.

## Banco ampliado (bloque 5)

Cada patch nuevo se calibró midiendo un acorde de cuatro notas (Cmaj7) a velocity
0.8 durante 4 s; ningún pico debe llegar al limitador (techo 0.891). Los picos
medidos están en `docs/SESSION_STATUS.md`. Aun así, el juicio final es de oído:

### Warm Pad

- [ ] Ataque lento sin bombeo; el acorde se asienta y no "respira" de forma
      incómoda con el LFO lento.
- [ ] El brillo es cálido y redondo, no embarrado; al subir cutoff aparece aire
      sin estridencia.
- [ ] En un acorde cerrado (C3–B3) no hay batido metálico del unison.

### Glass Pad

- [ ] Timbre cristalino y brillante, pero no piercing en notas altas.
- [ ] El ataque y el release largos no dejan cola infinita ni bombeo.
- [ ] Las cuatro notas se distinguen; no se funden en una masa sin altura.

### Dark Drone

- [ ] Grave y oscuro, con el sub audible sin embarrar el rango medio.
- [ ] El LFO muy lento mueve el filtro de forma perceptible pero no cíclica y
      molesta.
- [ ] No satura ni "ratea" al mantener el acorde varios segundos.

### Soft Pluck

- [ ] Ataque suave y decay corto; se reconoce como pulsación, no como pad.
- [ ] Es el patch más silencioso del banco: ¿el nivel relativo es usable o hay
      que subir `MASTER`?
- [ ] El release corto no chasquea al soltar rápido.

### Pizz Synth

- [ ] Ataque inmediato y resonancia controlada; no silba ni "pía" en el decay.
- [ ] En acorde, las notas no se enmascaran entre sí.

### Lead Saw

- [ ] Brillo de sierra recognoscible, sin aliasing audible en el registro alto.
- [ ] El vibrato es sutil; no convierte el lead en un coro desafinado.
- [ ] Con cuatro notas sigue siendo un lead, no un muro de sierras.

### Square Lead

- [ ] Timbre hueco/cuadrado clásico, con el ancho de pulso audible.
- [ ] No hay zumbido de baja frecuencia ni clipping perceptible.
- [ ] El vibrato no desestabiliza el acorde.

### Organ Drawbar

- [ ] Ataque y release casi instantáneos, sin clic.
- [ ] Timbre estable y plano (órgano), sin bombeo de filtro.
- [ ] Las notas agudas no se vuelven estridentes; el sub no embarulla.

### Electric Keys

- [ ] Timbre de piano eléctrico, con decay natural y sin "campana" metálica.
- [ ] La velocity (si se prueba con ratón/piano) cambia volumen y brillo de
      forma creíble.
- [ ] El sustain bajo no deja la nota colgada al soltar.

### Sub Bass

- [ ] Fundamental grave y limpia; el sub no retumba ni satura el limitador.
- [ ] En acorde (poco habitual para un bajo) no hay barro ni cancelación.
- [ ] El ataque corto no chasquea.

### Air Texture

- [ ] Textura de aire/ruido filtrado, no un soplido estático.
- [ ] El LFO lento y el filtro crean movimiento sin silbar.
- [ ] El ruido no tapa las notas ni suena a "radio mal sintonizada".

### Brass Stab

- [ ] Ataque rápido con cuerpo de metal, sin clic.
- [ ] El decay corto deja un stab reconocible; el release no corta en seco.
- [ ] En acorde no resulta áspero ni distorsionado.

## Archivos de patch de usuario

- [ ] Guardar un patch en `SETUP > PATCH FILES > SAVE`, reiniciar y cargarlo:
      suena igual que antes de cerrar.
- [ ] `DUP` crea una copia y `DEL` la borra; el selector de archivos se
      actualiza sin cerrar el diálogo.
- [ ] Cargar un archivo editado a mano con un valor fuera de rango no rompe la
      app y suena con el valor clampado.
- [ ] Cargar un JSON corrupto muestra un error legible y no deja el motor mudo.

## Efectos (Etapa 5A)

- [ ] Con todos los slots bypass (`Chain:` muestra `-`), el timbre y el nivel
      son los mismos que en la Etapa 4 salvo 5 ms de latencia.
- [ ] `Q` + `B` activa reverb. Con `4` sube la mezcla: no hay peine ni cambio de
      nivel brusco al pasar de seco a mojado.
- [ ] Reverb: decay corto/largo, damping alto/bajo y pre-delay de 0 a 200 ms se
      oyen como parámetros musicales, sin zumbidos ni colas que crezcan.
- [ ] `W` + `B` activa delay. A 100 ms, un golpe en el canal izquierdo responde
      primero a la izquierda y luego a la derecha (ping-pong).
- [ ] Delay con feedback cerca de 0.95: repite muchas veces sin crecer ni
      volverse inestable; el filtro del lazo oscurece las repeticiones.
- [ ] `E` + `B` activa chorus: el vibrato es suave, no desafina el acorde y el
      estéreo se abre sin cancelarse en mono.
- [ ] `R` + `B` activa drive: sube `1` (amount) y el timbre se satura sin
      volverse más fuerte; `2` (tone) oscurece sin caer en barro; a plena escala
      no hay chasquidos ni aliasing audible.
- [ ] `T` + `B` activa el filtro de cadena: LP/HP/BP con `3` (mode) y máxima
      resonancia no silba de forma incontrolada.
- [ ] `N` cambia el tipo de un slot mientras suena: el crossfade no hace clic y
      la latencia de la cadena no salta de forma audible.
- [ ] Al pasar un slot a bypass con reverb o delay, la cola se apaga de forma
      natural; no se corta en seco.
- [ ] El limitador no bombea de forma molesta con acordes sostenidos ni deja
      pasar ningún pico; no hay distorsión digital.
- [ ] La consola muestra `Chain:` y la latencia total (240 muestras con la
      cadena por defecto) de forma coherente con el resumen al salir.

## Efectos nuevos (Etapa 5B)

- [ ] `N` recorre los once tipos sin huecos: reverb, delay, chorus, drive,
      filtro, flanger, phaser, tremolo, vibrato, eq, lofi; el nombre del slot y
      la franja de parámetros coinciden con el tipo elegido.
- [ ] Flanger: el barrido se oye como peine que sube y baja, sin silbidos ni
      ganancia que suba al mover `feedback`.
- [ ] Phaser: barrido suave y continuo a velocidades lentas; con `depth` a 1 y
      `feedback` a 0.9 no se vuelve inestable ni zumba.
- [ ] Tremolo: con `depth` a 1 corta por completo; `shape` cuadrado se oye como
      puerta rítmica y `stereo` aumenta el vaivén sin cancelar en mono.
- [ ] Vibrato: el pitch oscila sin que se note el retardo; con `depth` a 5 ms
      suena desafinado de forma evidente pero no rota.
- [ ] EQ: +12 dB en `low` engorda sin retumbe y −12 lo adelgaza; el pico de
      `mid` se mueve con `midFrequency` y el agudo no se vuelve estridente.
- [ ] Lo-Fi: a 4 bits y `rate` bajo se oye crujido y suciedad de vinilo;
      `noise` y `wow` añaden siseo y desafinación sin tapar la nota.
- [ ] Ningún efecto nuevo hace clic al mover sus parámetros ni al cambiar de
      tipo; el limitador no bombea con el flanger a feedback máximo.

## Pendiente también de verificación externa

- [ ] GitHub Actions en verde para el commit entregado (el entorno local no
      tiene acceso al remoto).
- [ ] Escucha prolongada de 10 minutos por patch para descartar fatiga o
      artefactos intermitentes.
