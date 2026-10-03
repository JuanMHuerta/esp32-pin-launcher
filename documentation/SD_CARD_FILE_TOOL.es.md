# Herramienta de archivos SD

[English](SD_CARD_FILE_TOOL.md) · [Español](SD_CARD_FILE_TOOL.es.md)

El menú expone la tarjeta microSD montada por USB Serial/JTAG. El cliente Python
permite consultar y copiar archivos sin retirar la tarjeta.

## Disponibilidad

El servicio pertenece al firmware del menú raíz. No está disponible mientras
corre una app. Mantené BOOT durante 1,5 segundos y soltalo para volver al menú
antes de usar el cliente.

La herramienta es [`tools/sdcard.py`](../tools/sdcard.py). Abrir el puerto serie
puede reiniciar la placa; esperá el arranque y el saludo del protocolo. Si hay
más de un ESP32 conectado, indicá `--port`.

## Cambios de firmware

Antes de cambiar SD, pantalla u otro código de hardware, leé el
[contrato de la placa](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md) y las
[fuentes de validación](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.es.md).
Después de un cambio, compilá e instalá el menú con el entorno ESP-IDF del
proyecto y comprobá la tarjeta:

```sh
source "$IDF_PATH/export.sh"
idf.py -C . build
idf.py -C . -p /dev/ttyACM0 flash

python3 tools/sdcard.py --port /dev/ttyACM0 info
python3 tools/sdcard.py --port /dev/ttyACM0 list /
```

Instalar sólo el menú alcanza para probar el servicio si la distribución de
particiones no cambió. Si creció la imagen o cambiaron direcciones, usá el
script raíz para reinstalar la tabla y todas las imágenes juntas.

## Comandos

El cliente requiere `pyserial`. Puede encontrar la placa mediante
`/dev/serial/by-id/` o `/dev/ttyACM*`; `--port` elimina esa ambigüedad.
`--timeout` permite ajustar la espera para una conexión lenta u ocupada.

```sh
python3 tools/sdcard.py info
python3 tools/sdcard.py list /
python3 tools/sdcard.py put local-file.bin /remote/file.bin
python3 tools/sdcard.py put local-directory /remote/directory
python3 tools/sdcard.py get /remote/file.bin local-file.bin
python3 tools/sdcard.py get /remote/directory local-directory
python3 tools/sdcard.py mkdir /remote/new/directory
python3 tools/sdcard.py rm /remote/file.bin
python3 tools/sdcard.py rm -r /remote/directory
```

`put` y `get` copian directorios recursivamente. `mkdir` crea los directorios
padre que faltan. `rm` elimina un archivo o un directorio vacío; para uno con
contenido se necesita `rm -r`. El cliente da error si hay varios dispositivos
serie posibles, en lugar de elegir uno arbitrariamente.

Las rutas remotas parten de la raíz de la tarjeta; `/` representa esa raíz.
El cliente rechaza componentes vacíos, `.` y `..`. El firmware también rechaza
barras invertidas, dos puntos, controles y rutas que escapen de la raíz.
El cliente convierte barras invertidas en barras normales: usá rutas POSIX
para evitar depender de esa conversión. No se puede borrar la raíz.
La configuración FAT admite nombres largos UTF-8.

Las escrituras usan un archivo temporal `.mpfs.tmp` y lo renombran al terminar;
una transferencia interrumpida no reemplaza el destino por un archivo parcial.
El menú nunca formatea automáticamente la tarjeta.

## Restricciones de la placa

El proyecto usa la Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596. La placa
probada informa revisión de silicio ESP32-S3 v0.2. La configuración SD sigue
el ejemplo SDMMC de un bit `VersionControl_V2` de Waveshare:

| Señal | GPIO |
| --- | ---: |
| CLK | 9 |
| CMD | 42 |
| D0 | 8 |

Este mapeo coexistió con la pantalla SH8601 y evita su reloj en GPIO47.
Para otra revisión o SKU, comprobá primero el contrato, las fuentes y el
ejemplo del fabricante correspondiente. La revisión de silicio por sí sola
no identifica una revisión de PCB.

La tarjeta se monta en `/sd` con `format_if_mount_failed=false`. Cambiar el bus,
pines, ajustes FATFS o relación con la pantalla es un cambio de hardware.
Actualizá los documentos de la placa y registrá una lección reutilizable cuando
la prueba produzca un hallazgo nuevo.

## Responsabilidades y protocolo

- [`main/sdcard.c`](../main/sdcard.c): montaje SD, validación de rutas,
  operaciones y protocolo binario.
- [`main/sdcard.h`](../main/sdcard.h): API SD para el menú.
- [`main/main.c`](../main/main.c): USB Serial/JTAG, comandos del menú y entrada
  del protocolo sin interferir con `1`–`9` ni `D`.
- [`tools/sdcard.py`](../tools/sdcard.py): cliente sincronizado con el firmware.

La cabecera tiene 16 bytes en little-endian: `<4sBBBBII>`.

| Campo | Significado |
| --- | --- |
| magic | `MPFS` |
| version | `1` |
| type | solicitud `0`, respuesta `1`, datos `2` |
| opcode | identificador de operación |
| flags | `MORE=1`: siguen datos; `END=2`: transferencia terminada |
| status | código de resultado |
| payload length | bytes después de la cabecera |

Operaciones: `HELLO=1`, `STAT=2`, `LIST=3`, `READ=4`, `WRITE_BEGIN=5`,
`WRITE_DATA=6`, `WRITE_END=7`, `WRITE_ABORT=8`, `MKDIR=9` y `DELETE=10`.
El estado `0` indica éxito. Los demás códigos indican, en orden: no encontrado,
solicitud/ruta inválida, error de E/S, tarjeta sin montar, ocupado, ya existe,
directorio no vacío, sin espacio y error de protocolo.

Las rutas usan UTF-8: máximo 255 bytes por componente y 507 bytes para la ruta
relativa normalizada. Se rechazan componentes vacíos, `.`/`..`, controles y dos
puntos. Cada trama admite hasta 4096 bytes de carga útil. `READ` y `LIST` pueden
enviar una respuesta, tramas de datos y una respuesta final. Las escrituras
usan `WRITE_BEGIN`, varios `WRITE_DATA` y `WRITE_END`. Un nuevo `HELLO` aborta
una escritura incompleta. Si cambia el protocolo, actualizá juntos el C,
el cliente Python, ambas guías y las pruebas.

## Verificación

Usá directorios únicos en la computadora y en la tarjeta para evitar reemplazar
archivos existentes. Definí `PORT` con el dispositivo serie de tu placa y,
desde la raíz, ejecutá:

```sh
work_dir=$(mktemp -d)
remote_dir="/repo-check-$(basename "$work_dir")"
trap 'python3 tools/sdcard.py --port "$PORT" rm -r "$remote_dir"; rm -rf -- "$work_dir"' EXIT
python3 tools/sdcard.py --port "$PORT" mkdir "$remote_dir"
python3 tools/sdcard.py --port "$PORT" put documentation "$remote_dir/documentation"
python3 tools/sdcard.py --port "$PORT" get "$remote_dir/documentation" "$work_dir/documentation"
diff -ru documentation "$work_dir/documentation"
```

La limpieza al salir elimina ambos directorios temporales. Para un cambio de
firmware o protocolo, ejecutá también `git diff --check` y
`python3 -m unittest discover -s tools/tests`. Comprobá al menos una ida y vuelta
binaria con un hash o `cmp`. Dejá la raíz SD como estaba antes de la prueba.

## Problemas frecuentes

- `No unique ESP32 serial device found`: indicá `--port` y cerrá otros monitores.
- Tiempo agotado en `HELLO` o un comando: volvé al menú, esperá el reinicio y
  reintentá. El registro del menú distingue fallos de arranque y de protocolo.
- `SD card is not mounted`: revisá el registro y el contrato de la placa;
  conservá el montaje sin formato automático.
- Error de ruta: usá rutas POSIX desde la raíz, sin `..`, barras invertidas
  ni caracteres de control.
- Archivo `.mpfs.tmp` restante: revisalo antes de borrarlo. Es un resto de
  transferencia interrumpida, no necesariamente el destino esperado.
