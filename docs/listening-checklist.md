# Checklist de escucha (pendiente de verificación humana)

Ninguna de estas comprobaciones se puede hacer con tests: requieren oído. El
código y las mediciones offline están verificados; esta lista es lo que queda
por confirmar en una sesión de escucha real.

## Preparación

- Ejecutar `drumaspro.exe --buffer=256 --rate=48000` con el volumen del sistema
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

## Pendiente también de verificación externa

- [ ] GitHub Actions en verde para el commit entregado (el entorno local no
      tiene acceso al remoto).
- [ ] Escucha prolongada de 10 minutos por patch para descartar fatiga o
      artefactos intermitentes.
