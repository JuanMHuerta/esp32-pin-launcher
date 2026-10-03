# Miso

[English](README.md) · [Español](README.es.md)

Una mascota en un claro del bosque con hongos, helechos, mariposas y luciérnagas.
Las formas grandes en menta y crema, los bordes oscuros, los ojos expresivos y
la bufanda naranja permiten distinguir a Miso a distancia. No hay texto en
pantalla, tampoco durante las siestas.

Un ciclo de seis minutos recorre día, atardecer, noche y amanecer. El sol baja y
sale una luna creciente; bosque, suelo y personaje cambian de paleta juntos.
Pasan nubes, titilan estrellas y al anochecer las luciérnagas reemplazan a las
mariposas. Miso conserva suficiente brillo para verse de noche.

Miso camina, encuentra y come bayas, huele flores, mira insectos, se acicala,
se estira, baila, saluda, persigue y duerme. Un ciclo de actividades barajadas
da un turno a cada comportamiento autónomo. Las siestas nocturnas duran más;
al despertar se estira.

![Miso y sus poses durante el día y la noche](preview.gif)

El GIF acelera el ciclo de seis minutos y demuestra las poses. También están
las [cuatro fases de luz](preview-day-night.png) y la
[hoja de acciones a media resolución](preview-actions.png).

| Acción | Reacción |
| --- | --- |
| Tocar a Miso | Saltos y corazones |
| Tocar un espacio vacío | Dejar una baya para que coma |
| Deslizar el dedo | Perseguir una mariposa de día o luciérnaga de noche |
| Mantener el dedo 0,8 segundos | Siesta; repetir o tocar para despertar |
| Inclinar | Mirada, orejas, cuerpo y escenario acompañan |
| Sostener una inclinación mayor | Equilibrio con patas extendidas; saludo al estabilizar |
| Sacudir suavemente | Alternar sorpresa, baile y persecución; pausa de seis segundos |
| Tocar BOOT | Alternar brillo medio, alto y bajo |
| Mantener BOOT 1,5 segundos y soltar | Volver al menú |

Dejá la placa quieta durante el primer medio segundo para fijar la inclinación
neutra. El brillo empieza en medio. Dormir es una animación; la placa sigue activa.

## Desarrollo

La simulación dibuja en 134 × 60 y escala 4×. `main/pet.c` maneja comportamiento,
`main/paint.c` dibuja, `main/input.c` reconoce gestos y `main/board.c` controla
las tareas de pantalla y sensores.

Desde la raíz:

```sh
./tools/test.sh --app pet-pin
python3 tools/make_previews.py pet-pin
python3 firmwares/pet-pin/tools/preview.py --publish
idf.py -C firmwares/pet-pin build
```

Usá el [script raíz](../../build-and-flash.sh) para instalar. Las pruebas cubren
24 horas simuladas, actividades sin intervención, alimentación, inclinación y
recuperación, protección de siestas, variedad de sacudidas, ciclo de luz, tacto,
filtros, límites del dibujo y orden de bytes. El
[registro histórico](../../documentation/APP_VALIDATION.md) contiene las
observaciones de la placa.

Dentro de esta app, `python3 tools/preview.py` exporta catorce animaciones,
hojas de luz y acciones, y un video a `artifacts/`, excluido de Git.
`--publish` actualiza los PNG incluidos. Requiere Pillow y FFmpeg con VP9.
Para regenerar el GIF largo desde la raíz:
`python3 tools/make_previews.py pet-pin --seconds 28`.

## Consola de diagnóstico

Mientras corre Miso, enviá comandos terminados en salto de línea por USB Serial/JTAG:

```text
status
state idle
state walk
state sniff
state eat
state sleep
state love
state play
state surprise
state wave
state groom
state stretch
state dance
state look
state balance
tap 67 30
swipe 110 30
hold
shake
brightness 0
capture
bars
auto
```

Las coordenadas son píxeles lógicos de 134 × 60. `state` mantiene una actividad
hasta diez segundos; caminar puede terminar al llegar. `capture` envía el último
cuadro lógico en hexadecimal RGB565 y pausa brevemente la animación. `bars`
muestra colores de diagnóstico; `auto` devuelve el comportamiento autónomo.
La telemetría expresa coordenadas, aceleración e inclinación como enteros
escalados. `PERF` informa tiempos y uso de memoria cada diez segundos.

Después de reiniciar, el controlador táctil puede esperar una interrupción
de contacto para despertar. Un NACK inicial no deshabilita el tacto: el firmware
mantiene la interrupción activa. Consultá el
[contrato de la placa](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md).
