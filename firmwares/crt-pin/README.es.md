# CRT / ATLAS VT-220

[English](README.md) · [Español](README.es.md)

Una estación orbital ficticia arranca e inicia sesión una vez, y deja su consola
de estilo Unix funcionando indefinidamente. Una lista de 817 eventos dura unos
5 minutos y 18 segundos y repite seis secuencias:

- **Operaciones:** procesos, memoria, disco, red, registros, sincronización,
  observación, correo y mensajes de guardia.
- **Mantenimiento:** diagnósticos, comunicación serie, retransmisiones,
  watchdog, reloj alternativo, ROM y fallos.
- **Telemetría:** frecuencias, antena, paquetes, CRC, sensores, portadora,
  almacenamiento temporal y clima.
- **Red:** rutas, trazas, búferes, loopback, pruebas de saltos, prioridades
  y mantenimiento de conexiones.
- **Archivos:** manifiestos, hashes, compresión, transferencias, recibos,
  retención, paridad y reparación de bloques duplicados.
- **Señales:** espectro, ganancia, frecuencia, órbitas, muestras I/Q,
  supresión de interferencias y calibración.

Trazas ASCII se animan durante las observaciones y diagnósticos. Etiquetas,
números y archivos incluyen pequeñas referencias de ciencia ficción.
Los comandos se escriben a unos 55 caracteres por segundo; las respuestas
llegan en ráfagas con pausas, avisos y barras de progreso. Cada secuencia
conserva el historial visible. Los comandos y datos son ficción programada:
no ejecutan una shell ni acceden al almacenamiento, red o sensores de la placa.

![Consola de la estación](preview-0.gif)

Cada inicio o reinicio manual elige fósforo verde o ámbar al azar y conserva
ese tono durante la sesión. La terminal tiene 39 columnas y doce filas que
ocupan toda la altura del vidrio. Líneas de barrido, brillo, curvatura,
atenuación, grano, banda de refresco, oscilación y cursor de bloque producen el
aspecto CRT. Un marco gris rodea el vidrio. La expansión inicial se repite sólo
al reiniciar explícitamente la consola.

Tocá BOOT para reiniciar en la siguiente secuencia y sortear otro color, que
puede repetirse. Mantenelo 1,5 segundos y soltalo para volver al menú. El atajo
USB es `9`; en la colección completa está en la segunda página y ocupa `ota_8`.
La dirección sale de `partitions.csv`; las selecciones web reasignan subtipos OTA.

La reproducción usa el tiempo de eventos para conservar velocidad entre fps
distintos. Un reloj de 64 bits evita repetir el inicio al superar el límite de
milisegundos de 32 bits. La terminal no asigna memoria durante la reproducción.
La escena RGB565 de 268 × 120 se expande a 536 × 240 con doble búfer DMA.
No necesita PSRAM ni SD.

## Desarrollo

Desde la raíz:

```sh
tools/test_scenes.sh
python3 tools/preview_scene.py crt-pin --preset 0 --start 44 --seconds 14
source /path/to/esp-idf/export.sh
idf.py -C firmwares/crt-pin build
```

Las pruebas cubren glifos, comandos, progreso, límites, trazas, pie de pantalla,
historial entre las seis secuencias, cientos de ciclos, consistencia entre pasos
de tiempo, reinicios, ambos colores y límites del renderizador.
`SANITIZERS=address,undefined` agrega comprobaciones si están disponibles sus
bibliotecas. Los presets `0`–`5` eligen secuencias; las vistas usan verde para
pares y ámbar para impares. El dispositivo sortea el color independientemente.

Con la placa en el menú:

```sh
python3 tools/check_app.py crt-pin --seconds 360 --all-profiles
```

La prueba inicia la app, observa las seis secuencias y comprueba fallos, fps
y memoria. Instalá con el script raíz `build-and-flash.sh`; `idf.py flash` de
la app independiente reemplaza la tabla de particiones de la colección.

Más animaciones: [mantenimiento ámbar](preview-1.gif),
[telemetría](preview-2.gif), [red](preview-3.gif), [archivos](preview-4.gif)
y [señales](preview-5.gif). Usan el renderizador C del dispositivo.
