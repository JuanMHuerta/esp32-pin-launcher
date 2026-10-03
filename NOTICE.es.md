# Licencias y origen de los recursos

[English](NOTICE.md) · [Español](NOTICE.es.md)

El código, la documentación y el arte de ESP32 Pin Launcher se distribuyen bajo
GPL-3.0-only. El texto completo está en [LICENSE](LICENSE). Los archivos fuente
incluyen identificadores SPDX; las tablas generadas heredan la licencia de sus
fuentes.

ESP-IDF y los componentes descargados son dependencias externas. Sus licencias
provienen de sus distribuciones originales:

| Dependencia | Versión usada | Licencia |
| --- | --- | --- |
| ESP-IDF | 5.5.1 | Apache-2.0, con componentes de otras licencias |
| `espressif/esp_lcd_sh8601` | 2.0.1~1 | Apache-2.0 |
| `espressif/cmake_utilities` | 0.5.3 | Apache-2.0 |

El gestor de componentes las descarga según los manifiestos y archivos de
versiones fijadas. Al distribuir firmware, conservá los textos de licencias y
avisos originales que correspondan junto con el código y las instrucciones
de compilación.

El instalador web incluye `esptool-js` 0.7.0 (Apache-2.0), `pako` 2.x (MIT y Zlib),
`atob-lite` 2.x (MIT) y `spark-md5` 3.0.2 (MIT). Las versiones exactas están en
[web/package-lock.json](web/package-lock.json). El JavaScript conserva los
comentarios de licencia originales y el sitio incluye sus textos en
`THIRD_PARTY_LICENSES.txt`.
El empaquetador incluye licencias y avisos del firmware en
`FIRMWARE_LICENSES.txt`, el código fuente del proyecto de esa versión en
`source.tar.gz` y las ubicaciones de fuentes de dependencias en `SOURCE.txt`.

Los patrones de Conway y las condiciones iniciales de la órbita en ocho de
Three Body son datos matemáticos; el código conserva las referencias. Maze
interpreta y dibuja el aspecto de un salvapantallas clásico. Sus texturas y
objetos se generan en C; no se incluyen el salvapantallas ni recursos de Microsoft.

Miso, Lumen, Conway, Fluid, Maze y CRT dibujan sus gráficos por procedimientos.
Dungeon y Wayfarer incluyen arte generado con IA y procesado con las herramientas
del repositorio, además de dibujo procedural. Se incluyen los PNG editables;
consultá las notas de recursos de [Dungeon](firmwares/dungeon-pin/assets/ART_DIRECTION.es.md)
y [Wayfarer](firmwares/wayfarer-pin/assets/ART_DIRECTION.es.md).
