# Validación del repositorio

[English](REPOSITORY_VALIDATION.md) · [Español](REPOSITORY_VALIDATION.es.md)

## Preparación de publicación — 2026-10-03

Esta revisión cubre las nueve apps, documentación bilingüe y el instalador USB
estático. Se usaron ESP-IDF 5.5.1, GCC 16.2.1, Python 3.14.7, Node.js 22.23.1,
Ruff 0.16.10 y clang-format 23.1.2. Los componentes de firmware siguen fijados
por los manifiestos y archivos de bloqueo.

### Código y previews

- Pasaron las pruebas de las nueve apps y los gráficos compartidos, con
  advertencias C tratadas como errores, AddressSanitizer y
  UndefinedBehaviorSanitizer. También pasó la ejecución independiente de
  comportamiento indefinido en modo trap.
- Pasaron dieciséis pruebas Python de asignación flash, protocolo SD y
  empaquetado. Cubren hashes, archivo de fuentes, avisos de licencia,
  reproducibilidad, tablas obsoletas y ejecución sin metadatos Git.
- Pasaron lint y formato Python, formato C escrito a mano, sintaxis shell y
  `git diff --check`. Las tablas generadas no se formatean como código manual.
- Se comprobaron enlaces locales, guías y README de apps en ambos idiomas,
  identificadores de licencia, orden de catálogo/atajos y tamaños de archivos.
- Los dos README principales incluyen los nueve previews animados. Todos tienen
  frames de 536 × 240 y duraciones positivas; duran entre seis y 32 segundos.
  Se revisaron frames representativos y el instalador en tamaños de escritorio
  y teléfono. Son renderizados en la computadora, no grabaciones del panel.

Las bibliotecas de sanitizers se extrajeron en una carpeta local ignorada por
Git. Las pruebas y comprobaciones de tablas usan directorios temporales únicos
y los eliminan al terminar.

### Firmware y publicación web

El launcher y las nueve apps compilaron con `./build-and-flash.sh --build-only`.
La distribución calculada coincidió con la tabla compilada del conjunto
completo. El sitio incluye firmware con nombres derivados de SHA-256, nueve
previews, licencias externas, ubicaciones de dependencias y las fuentes del
proyecto correspondientes a la publicación.

El archivo de fuentes se extrajo en un directorio temporal único, sin
metadatos Git, builds, componentes descargados ni archivos `sdkconfig`
generados. Desde esa copia pasaron las pruebas de la computadora, las
compilaciones del launcher y las nueve apps, empaquetado sin Git, `npm ci`,
formato y pruebas unitarias web, comprobación de tablas y build del sitio
estático. Después se eliminó la copia temporal.

Pasaron nueve pruebas unitarias web y siete pruebas de navegador Chromium.
Cubren las 511 selecciones no vacías, límites e integridad, fallos de escritura
y reset, cierre del puerto, cancelación, errores numéricos de permisos del
navegador, estados y errores traducidos, idioma guardado, subrutas de
repositorio y pantallas estrechas. Las pruebas de navegador usan firmware
sintético y no escriben en una placa.

La comprobación de la publicación real analizó, verificó y reprodujo las
511 tablas con la implementación Python independiente de ESP-IDF. También
comprobó tamaño, SHA-256 y cabecera ESP32-S3 de cada imagen, además del tamaño
y hash del archivo de fuentes.

### Placa conectada

Se usó la Waveshare ESP32-S3-Touch-AMOLED-1.91 del proyecto, SKU 28596,
con silicio ESP32-S3 v0.2, flash de 16 MB, PSRAM integrada de 8 MB y USB
Serial/JTAG nativo. La revisión del silicio no identifica la revisión del PCB;
no se aplicó este pinout a otra placa.

El código de producción `web/src/install.js` y el esptool-js fijado instalaron
las imágenes reales mediante un adaptador serial local con la interfaz de
streams de Web Serial. Todas las escrituras pasaron verificación MD5 en la
placa. Esto comprueba el instalador y el protocolo ROM/stub en hardware real.
El selector nativo y los permisos USB del navegador se cubrieron por separado
con pruebas simuladas.

| Selección instalada | Resultado observado |
| --- | --- |
| Miso + CRT | Dos apps detectadas; atajo `1` omitido ignorado; Demo Miso → CRT → Miso cada cinco minutos, en `0x80000` y `0xe0000` |
| Sólo Fluid | Una app detectada; atajo `1` omitido ignorado; Demo reinició Fluid a los cinco minutos en `0x80000` |
| Las nueve apps | Nueve apps detectadas; el atajo USB `9` arrancó CRT en la dirección generada `0x3e0000` |

No aparecieron panic, abort ni brownout. El switcher final aplaza la rotación
Demo mientras BOOT esté pulsado, siguiendo la regla existente de reset con
GPIO0. Las pruebas de Fluid solo y del conjunto completo usaron esa compilación.
No se comprobaron pulsaciones físicas prolongadas ni los gestos de todas las apps.

El usuario confirmó las comprobaciones físicas de Fluid: sin huecos oscuros
ni franja en V en reposo, desprendimiento rápido al inclinar y movimiento
visible al llevar la placa como al caminar. La
[revisión de física](../firmwares/fluid-pin/docs/physics-review.es.md)
registra el resultado y cierra sus comprobaciones pendientes.

Antes de instalar se guardaron los 16.777.216 bytes de flash. Al terminar se
restauró esa imagen, incluidos NVS y selección OTA, y una comprobación
independiente `verify_flash` del contenido completo coincidió. SHA-256 del backup:
`0bd3bd19546aa9275b110ab4728e6d0eff6435f64183eeebc8da56781905d37b`.
Miso arrancó correctamente tras la restauración, con su brillo original.
El backup y los registros quedan en la carpeta ignorada
`artifacts/release-review-20261003/`. No se cambiaron archivos SD ni se obtuvo
una nueva lección reutilizable de hardware.

El [workflow manual de Pages](../.github/workflows/web-flasher-pages.yml) y la
generación del artefacto en CI están preparados. Estos resultados son locales;
no se afirma una ejecución alojada en GitHub Actions ni una publicación pública.

## Revisión anterior — 2026-10-02

La revisión anterior comprobó las nueve apps tras quitar MECH y añadir GPL v3.
Pasaron pruebas, sanitizers, doce pruebas Python, compilación de assets,
formato y enlaces. El launcher y las nueve apps compilaron tanto en el árbol
de trabajo como en una copia limpia, sin builds, componentes descargados ni
`sdkconfig` generado. En esa revisión los GIF duraban entre seis y catorce
segundos. No se escribió ni probó hardware durante esa pasada; el
[registro en inglés](REPOSITORY_VALIDATION.md) conserva el detalle original.
