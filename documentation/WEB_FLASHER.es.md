# Instalador web por USB

[English](WEB_FLASHER.md) · [Español](WEB_FLASHER.es.md)

La [página web](../web/index.html) instala el menú y una selección de las nueve
apps en la Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596). Se necesita un
navegador de escritorio con Web Serial (Chrome o Edge), un cable USB de datos y
la página servida por HTTPS. El usuario no necesita una cuenta, ESP-IDF ni
compilar el firmware en su computadora.

La página ofrece inglés y español. Empieza con el idioma del navegador;
el selector recuerda la elección manual. Los registros de diagnóstico de
esptool conservan su idioma original.

## Compilar y probar localmente

Usá el entorno ESP-IDF 5.5.1 y Node.js 22 o posterior:

```sh
source /path/to/esp-idf/export.sh
./build-and-flash.sh --build-only
python3 tools/package_web_firmware.py
cd web
npm ci
npm test
npm run build
python3 -m http.server 8080 --directory dist
```

Abrí `http://localhost:8080`. Web Serial admite localhost; abrir directamente el
HTML o servirlo por HTTP desde otro equipo no habilita la instalación USB.
`web/dist/` contiene la versión estática completa: página, JavaScript, estilos,
vistas, manifiesto, firmware y licencias. Las rutas son relativas, por lo que
el sitio funciona bajo `/nombre-del-repositorio/` en GitHub Pages.
Las dependencias JavaScript se incluyen localmente; no se usa un CDN.
También se incluyen `source.tar.gz` con el código del proyecto usado para la
versión y `SOURCE.txt` con las fuentes de dependencias e instrucciones.

El empaquetador comprueba el firmware contra la tabla CSV y la tabla compilada,
verifica las cabeceras ESP32-S3 y copia las imágenes con nombres derivados de
su contenido y hashes SHA-256 en el manifiesto. Compilá todo el firmware antes
de empaquetar, especialmente después de cambiar el selector compartido.
Los archivos generados y la salida del sitio se excluyen de Git.
`FIRMWARE_LICENSES.txt` reúne licencias y avisos de ESP-IDF y componentes
descargados; `THIRD_PARTY_LICENSES.txt` conserva las licencias del JavaScript
distribuido.

## Selección, menú y Demo

La instalación siempre incluye el menú de fábrica y al menos una app. El
navegador genera una tabla con las apps elegidas, en orden de catálogo y con
asignaciones mínimas de 64 KiB. Los subtipos OTA son consecutivos desde `ota_0`;
las etiquetas conservan la identidad. No se pueden dejar huecos entre subtipos:
la selección de arranque de ESP-IDF usa el número de particiones OTA instaladas.

El menú encuentra las apps por etiqueta y muestra sólo las instaladas, junto
con Demo, en una o dos páginas. Los atajos USB mantienen sus identidades
(`1` Conway a `9` CRT); los de apps omitidas se ignoran. Demo inicia la primera
app y el selector compartido recorre las particiones OTA cada cinco minutos,
volviendo al inicio incluso si se instaló una sola app. Mantené BOOT durante
1,5 segundos y soltalo para volver al menú y detener Demo.

Antes de conectar al cargador, se descargan y verifican todas las imágenes con
SHA-256. Antes de escribir se comprueba que el chip sea ESP32-S3 y tenga 16 MB
de flash. [esptool-js de Espressif](https://github.com/espressif/esptool-js)
escribe los datos comprimidos y compara el MD5 de cada imagen con el dispositivo.
Se conservan los parámetros flash del bootloader compilado con ESP-IDF.

Instalar reemplaza la colección y borra NVS y los metadatos de selección OTA.
Esto reinicia los ajustes y el estado Demo; el siguiente arranque abre el menú.
No se borra toda la flash. Pueden quedar bytes de imágenes omitidas fuera de
las particiones nuevas, pero no aparecen en el menú ni en Demo. Cambiar la
selección instala otra colección completa y coherente. No se accede a la microSD.

Si falla el reinicio automático después de verificar la escritura, la página
pide presionar RESET. Los errores de conexión o escritura liberan el puerto
para reintentar. Para entrar manualmente al modo de descarga: mantené BOOT,
presioná y soltá RESET, soltá BOOT y elegí el puerto USB Serial/JTAG que aparece.
Cerrá los otros monitores serie.

## Publicar en GitHub Pages

El [workflow manual](../.github/workflows/web-flasher-pages.yml) compila con
ESP-IDF 5.5.1, empaqueta el sitio y publica el resultado:

1. Subí estos archivos a GitHub.
2. En **Settings → Pages**, elegí **GitHub Actions** como origen.
3. En **Actions → Publish web flasher**, ejecutá el workflow en la rama deseada.
4. Abrí la URL que informa el trabajo de publicación.

El flujo sigue la [documentación de GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).
La publicación es manual. Los pushes y pull requests generan un artefacto
descargable `web-flasher` mediante el workflow Checks.

## Validación

```sh
cd web
npm test
npm run format:check
npx playwright install chromium
npm run test:browser
```

Después de compilar y empaquetar, con ESP-IDF activado, ejecutá también
`npm run test:release` dentro de `web/`. Comprueba hashes de imágenes y del
archivo fuente, y las 511 tablas contra el lector y serializador de ESP-IDF.
La integración continua de firmware y el workflow de publicación lo ejecutan.

Las pruebas unitarias recorren las 511 selecciones no vacías, la serialización
de la tabla y su MD5, los límites y solapamientos, la integridad de descargas,
las comprobaciones de chip y memoria, los errores de escritura y reinicio, y
el cierre del puerto. Las pruebas de navegador cubren la selección vacía, la
cancelación del puerto, navegadores incompatibles, firmware ausente, rutas de
repositorio y tamaño de teléfono. Usan firmware sintético y no escriben en
una placa. La instalación real, el menú físico y la rotación de cinco minutos
requieren pruebas con hardware cuando cambia el firmware; el registro de
validación incluye las comprobaciones completadas en la placa.

El [registro de validación](REPOSITORY_VALIDATION.es.md) conserva los resultados
fechados y su alcance. Una prueba local del workflow no equivale a una ejecución
publicada en GitHub Actions.
