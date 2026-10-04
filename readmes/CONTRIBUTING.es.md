# Contribuir

[English](../CONTRIBUTING.md) · [Español](CONTRIBUTING.es.md)

Usá ESP-IDF 5.5.x y conservá la versión del controlador de pantalla fijada en
`main/idf_component.yml`. Antes de modificar código de hardware, leé el
[contrato de la placa](../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md).
Los detalles y las observaciones de hardware pertenecen a la documentación
compartida, para evitar copias distintas en cada app.

## Estructura

| Directorio | Contenido |
| --- | --- |
| `main/` | Menú y servicio de archivos SD |
| `common/` | Retorno al menú, Demo, gráficos compartidos y adaptador QSPI |
| `firmwares/` | Apps independientes, recursos y pruebas en la computadora |
| `tools/` | Distribución flash, vistas, pruebas y clientes serie |
| `documentation/` | Reglas de hardware, protocolo y registros de validación |
| `web/` | Instalador USB estático, compilación y pruebas de navegador |

La mayoría de las apps separa la simulación y el dibujo de su punto de entrada
ESP-IDF. Conservá esa separación para poder probarlas sin hardware. Regenerá
las tablas de recursos C desde sus PNG originales, en lugar de editarlas a mano.

Mantené sincronizados los README y las guías en inglés y español. Describí
directamente el comportamiento y conservá los nombres de apps, comandos y
campos del protocolo.

## Comprobaciones

Instalá `requirements-dev.txt` en un entorno virtual y ejecutá:

```sh
./tools/test.sh
python3 tools/check_repo.py
ruff check .
ruff format --check .
./tools/format.sh --check
./build-and-flash.sh --build-only
```

Para el instalador web, ejecutá `npm ci`, `npm test`, `npm run format:check` y
`npm run test:browser` dentro de `web/`. La
[guía del instalador](../documentation/WEB_FLASHER.es.md) explica cómo empaquetar
y probar una versión.

Las pruebas C tratan las advertencias como errores.
`SANITIZERS=address,undefined ./tools/test.sh` agrega comprobaciones cuando
están instaladas las bibliotecas necesarias.
`SANITIZERS=undefined SANITIZER_TRAP=1 ./tools/test.sh` permite comprobar
comportamiento indefinido sin su biblioteca de diagnóstico.
`./tools/test.sh --soak` incluye las pruebas largas de Fluid. Los binarios de
prueba usan directorios temporales únicos que se borran al salir.

Los cambios de hardware necesitan pruebas en la placa compatible. Registrá
placa y revisión, herramientas, comandos y resultados observados. Una vista
generada en la computadora no comprueba los colores de la pantalla, la
orientación táctil ni los sensores. Agregá una
[lección de hardware](../documentation/ESP32_DEVICE_LESSONS.es.md) sólo si aprendiste
algo nuevo y reutilizable.

## Formato y comentarios

Usá la configuración de clang-format y Ruff incluida. `./tools/format.sh`
formatea el C escrito a mano; `ruff format .` formatea Python. El formateador C
omite las tablas generadas. Dentro de `web/`, `npm run format` formatea JavaScript,
HTML y CSS. Los archivos fuente nuevos deben incluir
`SPDX-License-Identifier: GPL-3.0-only`.

Los comentarios deben explicar unidades, invariantes o por qué una solución
necesita cierta complejidad. Eliminá los que repiten la instrucción siguiente
o narran un cambio terminado. Documentá el comportamiento actual con comandos
reproducibles; conservá mediciones y direcciones antiguas en registros fechados.

## Generar vistas previas

Desde la raíz:

```sh
python3 tools/make_previews.py
python3 firmwares/render-pin/tools/render_previews.py
python3 firmwares/dungeon-pin/tools/make_preview.py
python3 firmwares/wayfarer-pin/tools/make_preview.py
python3 tools/preview_scene.py three-body-pin --preset 0 --seconds 14
python3 tools/preview_scene.py crt-pin --preset 0 --start 44 --seconds 14
```

El primer comando genera Conway, Fluid, Miso y Maze; los demás generan las apps
restantes. Se necesita Pillow; Lumen también usa FFmpeg. Las herramientas compilan
el mismo código C de simulación y dibujo que usa el firmware. Conservá un tamaño
de imágenes razonable para que el README cargue con comodidad en GitHub.

## Agregar una app

Creá un proyecto ESP-IDF en `firmwares/` con `sdkconfig.defaults`, dependencias
fijadas, README en ambos idiomas y una prueba en la computadora. Enlazá
`common/app_switcher.c` para volver al menú manteniendo BOOT. Registrá la app
en el catálogo de `main/menu_render.c`, su imagen en `tools/app_layout.py` y su
presentación en `tools/package_web_firmware.py` y `web/src/i18n.js`.
Demo descubre las particiones OTA instaladas. Mantené alineados el orden del
menú, los subtipos OTA y los atajos USB. Agregá la prueba a `tools/test.sh` y un
GIF generado por el renderizador a ambos README.

Compilá todas las imágenes antes de generar otra distribución. Si cambian las
direcciones, instalá la tabla y todas las imágenes juntas. Para cambiar el
servicio USB de archivos, consultá la [guía SD](../documentation/SD_CARD_FILE_TOOL.es.md):
la implementación C, el cliente Python y el documento del protocolo deben coincidir.

En un pull request, describí el comportamiento que cambió y las pruebas realizadas.
Para hardware, indicá la revisión comprobada y lo que sigue sin verificar.
Las contribuciones se distribuyen bajo GPL v3.
