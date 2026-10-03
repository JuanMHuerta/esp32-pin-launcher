# Lumen

[English](README.md) · [Español](README.es.md)

Un campo de estrellas 3D procedural para la Waveshare ESP32-S3-Touch-AMOLED-1.91
(SKU 28596). Pequeñas constelaciones se acercan entre capas de estrellas blancas,
con algunos acentos amarillos o rojos. Conexiones finas, destellos móviles y
halos se dibujan a 536 × 240 desde la geometría.

![Vuelo entre constelaciones](preview.gif)

[Inclinación, rotación y onda de luz simuladas](preview-motion.gif) ·
[Tres paletas](preview-palettes.png) · [Imagen fija](preview.png)

## Controles

- **Inclinar:** cambia la perspectiva y el paralaje de profundidad.
- **Rotar:** el eje X del giroscopio controla la vista horizontal y el Y la
  vertical. Rotar sobre la normal de la pantalla inclina el cielo. El movimiento
  no cambia el brillo.
- **Tocar:** envía una onda de luz desde el contacto y alterna Ice, Warm White
  y Silver, con un color de estrella y un acento de conexión a la vez.

Mantené BOOT 1,5 segundos y soltalo para volver al menú. La posición inicial
fija la referencia neutra del acelerómetro. El giroscopio responde enseguida y
vuelve suavemente a la vista gravitatoria en unos dos segundos. El roll se
integra con una pequeña zona muerta; no se sigue una orientación absoluta.
Sin IMU, continúan el vuelo y una deriva suave de cámara.

## Compilar

Desde la raíz, con ESP-IDF 5.5.x activado:

```sh
idf.py -C firmwares/render-pin build
./tools/test.sh --app render-pin
python3 firmwares/render-pin/tools/render_previews.py
```

Usá el [script raíz](../../build-and-flash.sh) para instalar. Las reglas de
pantalla y tacto están en el
[contrato de la placa](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md).

## Renderizado

- 640 estrellas independientes, incluidas 390 distantes sobre una esfera.
- 12 constelaciones procedurales, cada una con 6 estrellas y 5 conexiones.
- Campo horizontal aproximado de 96°; movimiento hasta 64° horizontal y 52°
  vertical. Posiciones subpíxel y conexiones con antialiasing.
- Máscaras precalculadas para ampliar estrellas sin desenfoque por píxel.
- Desvanecimientos cercanos y lejanos ocultan el reciclado; la profundidad
  determina la velocidad.
- Geometría proyectada una vez por cuadro y dibujo recortado a franjas de 60 filas.
- Dos búferes DMA internos superponen dibujo y transferencias QSPI. Hay una
  alternativa de un búfer. No se necesitan PSRAM ni un framebuffer completo.

El objetivo es 33 fps, con cuadros de 30 ms. Las mediciones históricas están en
[validación de apps](../../documentation/APP_VALIDATION.md).

`tools/render_previews.py` usa el renderizador C real, Pillow y FFmpeg. Los GIFs
corren a 15 fps. Las pruebas comprueban equivalencia entre cuadros y franjas,
límites, controles, movimiento y visibilidad de constelaciones. Las pruebas de
movimiento cubren inclinación, cada eje, recuperación y rechazo de tasas mínimas
con la placa quieta.
