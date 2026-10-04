# ESP32 Pin Launcher [![en](https://img.shields.io/badge/lang-en-red.svg)](../README.md) [![es](https://img.shields.io/badge/lang-es-yellow.svg)](README.es.md)

[![Checks](https://github.com/JuanMHuerta/esp32-pin-launcher/actions/workflows/ci.yml/badge.svg)](https://github.com/JuanMHuerta/esp32-pin-launcher/actions/workflows/ci.yml)

Nueve juegos y escenas animadas para la Waveshare ESP32-S3-Touch-AMOLED-1.91
(SKU 28596). Un menú permite elegir qué app iniciar. Cada una ocupa su propia
partición de la memoria flash y vuelve al menú con el botón BOOT.

Las apps funcionan sin conexión. Los gráficos y las simulaciones se ejecutan
en el ESP32; no hace falta un teléfono ni una red.

## Apps

Los GIFs de las apps y la vista del menú se generan en la computadora con código
C del firmware. Fluid usa una inclinación simulada; Miso muestra poses
programadas y un ciclo de día y noche acelerado. La grabación de Conway que
aparece más abajo se hizo en la pantalla física.

| Conway | Fluid | Miso |
| --- | --- | --- |
| [![Juego de la vida de Conway](../firmwares/conways-pin/preview.gif)](../firmwares/conways-pin/README.es.md) | [![Agua que responde a la inclinación](../firmwares/fluid-pin/preview.gif)](../firmwares/fluid-pin/README.es.md) | [![Miso en el bosque](../firmwares/pet-pin/preview.gif)](../firmwares/pet-pin/README.es.md) |
| Juego de la vida con patrones que se colocan al tocar. | Simulación de agua con partículas y control por movimiento. | Una mascota que reacciona al tacto, la inclinación y las sacudidas suaves. |

| Lumen | Dungeon | 3D Maze |
| --- | --- | --- |
| [![Vuelo entre constelaciones](../firmwares/render-pin/preview.gif)](../firmwares/render-pin/README.es.md) | [![Exploración y combate en una mazmorra](../firmwares/dungeon-pin/preview-combat.gif)](../firmwares/dungeon-pin/README.es.md) | [![Exploración automática de un laberinto](../firmwares/maze-pin/preview.gif)](../firmwares/maze-pin/README.es.md) |
| Un campo de estrellas controlado por movimiento y tacto. | Un recorrido autónomo en primera persona con combates. | Un laberinto generado que se explora siguiendo la pared derecha. |

| Wayfarer | Three Body | CRT |
| --- | --- | --- |
| [![Cabina de una nave y tráfico espacial](../firmwares/wayfarer-pin/preview.gif)](../firmwares/wayfarer-pin/README.es.md) | [![Tres soles luminosos y sus estelas orbitales](../firmwares/three-body-pin/preview-0.gif)](../firmwares/three-body-pin/README.es.md) | [![Consola de una estación orbital ficticia](../firmwares/crt-pin/preview-0.gif)](../firmwares/crt-pin/README.es.md) |
| Una cabina de pixel art con mundos, tráfico e iluminación cambiante. | Ocho encuentros gravitatorios de tres soles en un campo de estrellas. | Una terminal programada con brillo de fósforo y líneas de barrido. |

Cada vista enlaza a los controles, la implementación y los comandos de desarrollo
de la app.

## Menú

La vista recorre las apps instaladas y el modo Demo con la selección del menú.

![Menú con la selección recorriendo las apps](../main/menu-preview.gif)

## En el dispositivo

Esta grabación muestra el juego de la vida de Conway en la pantalla Waveshare.

![Juego de la vida de Conway en el dispositivo físico](../firmwares/conways-pin/device-demo.gif)

## Hardware

La placa compatible tiene 16 MB de flash, 8 MB de PSRAM, una pantalla AMOLED
horizontal de 536 × 240, un controlador táctil FT3168 y una IMU QMI8658C.
Las apps usan RGB565 y la interfaz QSPI compatible con SH8601. No requieren PSRAM.

Antes de cambiar pines, sensores o la inicialización de pantalla, consultá el
[contrato de la placa](../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md)
y las [fuentes y observaciones de hardware](../documentation/WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.es.md).
El soporte SD usa la configuración probada en la placa de este proyecto;
comprobá el mapeo antes de usar otra revisión de hardware.

## Compilar e instalar

El [instalador web](https://juanmhuerta.github.io/esp32-pin-launcher/) permite elegir apps e
instalarlas por USB desde el navegador. El menú y el modo Demo se incluyen
siempre y usan la selección instalada. La
[guía del instalador](../documentation/WEB_FLASHER.es.md) explica cómo probar el
sitio localmente y publicarlo en GitHub Pages.

Usá ESP-IDF **5.5.x**; los archivos de dependencias se generaron con **5.5.1**.
Instalá la cadena de herramientas ESP32-S3 siguiendo la
[guía de ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/get-started/index.html).
Los scripts necesitan Bash 5+, Python 3 y las herramientas de ESP-IDF.
Cloná el proyecto y activá ESP-IDF:

Si descargaste un archivo de fuentes, usá su directorio extraído y omití los
primeros dos comandos.

```sh
git clone https://github.com/JuanMHuerta/esp32-pin-launcher.git
cd esp32-pin-launcher
source /path/to/esp-idf/export.sh
./build-and-flash.sh --build-only
./build-and-flash.sh /dev/ttyACM0
```

Reemplazá `/dev/ttyACM0` por el puerto de tu placa. El último comando compila,
instala y verifica el menú y las nueve apps, y borra la selección de arranque OTA.
Reiniciá la placa para abrir el menú. Para entrar al modo de descarga ROM,
mantené BOOT, presioná y soltá RESET, y después soltá BOOT.

El script genera `partitions.csv` a partir del tamaño de las imágenes. Cada
imagen recibe el mínimo espacio contiguo en bloques de 64 KiB. Las direcciones
pueden cambiar cuando crece una imagen: después de un cambio de distribución,
instalá la tabla y todas las imágenes juntas. El comando `idf.py flash` de una
app independiente instala otra tabla de particiones; usá el script raíz para
esta colección.

Para consultar la distribución actual:

```sh
python3 tools/app_layout.py check --compiled
```

## Controles del menú

Un toque de BOOT selecciona la siguiente entrada. Mantenelo durante 0,7 segundos
y soltalo para iniciar la app. Dentro de una app, mantené BOOT durante
1,5 segundos y soltalo para volver al menú.

USB Serial/JTAG ofrece los mismos accesos:

| Comando | App | Partición en la colección completa |
| --- | --- | --- |
| `1` | Conway | `ota_0` |
| `2` | Fluid | `ota_1` |
| `3` | Miso | `ota_2` |
| `4` | Lumen | `ota_3` |
| `5` | Dungeon | `ota_4` |
| `6` | 3D Maze | `ota_5` |
| `7` | Wayfarer | `ota_6` |
| `8` | Three Body | `ota_7` |
| `9` | CRT | `ota_8` |
| `D` | Demo: cinco minutos por app instalada, en ciclo | — |

Mantener BOOT durante Demo detiene la rotación y vuelve al menú. Con una
selección instalada desde el navegador, el menú muestra sólo las apps elegidas.
Los atajos USB conservan las identidades de la tabla; los de apps omitidas se
ignoran. Los subtipos OTA se asignan consecutivamente a la selección.

## Desarrollo

Las pruebas en la computadora necesitan un compilador C, Make, Python **3.10+**
y Pillow. Algunas vistas previas también usan FFmpeg. Instalá las herramientas
de Python y formato en un entorno virtual:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
./tools/test.sh
```

Las pruebas cubren la simulación y el renderizado portable de cada app, los
gráficos compartidos, la distribución flash y el cliente SD. No necesitan una
placa. `./tools/test.sh --app maze-pin` prueba una app; `--soak` agrega las dos
simulaciones de 30 minutos de Fluid. `SANITIZERS=address,undefined ./tools/test.sh`
habilita comprobaciones de memoria y comportamiento indefinido si el compilador
dispone de las bibliotecas necesarias.

[CONTRIBUTING.es.md](CONTRIBUTING.es.md) explica la estructura del código,
el formato, la generación de vistas y las pruebas de hardware. Los
[registros de validación](../documentation/README.es.md) conservan las mediciones
históricas; sus direcciones antiguas no son instrucciones de instalación.

## Archivos en la tarjeta SD

Mientras corre el menú, `tools/sdcard.py` permite manejar una tarjeta microSD
con formato FAT por USB Serial/JTAG. Las apps no ofrecen este servicio.
El menú nunca formatea una tarjeta automáticamente. El cliente necesita pyserial.

```sh
python3 tools/sdcard.py --port /dev/ttyACM0 info
python3 tools/sdcard.py --port /dev/ttyACM0 list /
python3 tools/sdcard.py --port /dev/ttyACM0 put assets /assets
python3 tools/sdcard.py --port /dev/ttyACM0 get /assets ./downloaded-assets
```

La [guía SD](../documentation/SD_CARD_FILE_TOOL.es.md) describe los comandos,
límites de rutas y protocolo binario.

## Licencia

El código, la documentación y el arte del proyecto usan la
[GNU General Public License v3.0](../LICENSE), `GPL-3.0-only`.
Copyright © 2026 ESP32 Pin Launcher contributors.
Las dependencias conservan sus propias licencias; consultá [NOTICE.es.md](NOTICE.es.md).
