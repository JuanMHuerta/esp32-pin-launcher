# Validación histórica de la distribución flash — 2026-10-02

[English](FLASH_LAYOUT_VALIDATION.md) · [Español](FLASH_LAYOUT_VALIDATION.es.md)

Este resumen corresponde al registro anterior a la eliminación de MECH y al
menú actual de nueve apps. Las direcciones y atajos históricos no sirven como
instrucciones de instalación. El [registro original](FLASH_LAYOUT_VALIDATION.md)
conserva la tabla, los hashes y las observaciones completas.

Se usó una Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596, con silicio ESP32-S3
v0.2, flash de 16 MiB y USB Serial/JTAG nativo. La compilación empleó ESP-IDF
5.5.1 y los componentes fijados en los archivos de bloqueo.

Cada imagen recibió exactamente `ceil(tamaño / 65536) * 65536` bytes, sin
bloques adicionales para crecimiento. El launcher comenzaba en `0x20000`;
las apps eran contiguas. NVS y metadatos OTA conservaron sus posiciones.
La distribución de diez apps terminaba en `0x450000`, dejando 12.255.232 bytes
sin asignar. Es una medición de esa versión, no del conjunto actual.

Pasaron cinco pruebas de asignación, las comprobaciones de sintaxis y las
compilaciones del launcher y las diez apps. Se instalaron y verificaron todas
las imágenes; la tabla leída de la placa coincidió byte por byte con la
compilada. Se arrancó cada app con los atajos USB y se observaron sus registros
durante seis segundos. No hubo panic, abort ni bucles de reinicio.

Estas comprobaciones establecen arranque y direcciones, no revisión visual ni
uso prolongado. Al terminar se limpiaron los metadatos de selección OTA y se
dejó la placa en el launcher. Para una instalación actual, generá la tabla
con `build-and-flash.sh` y usá las direcciones que produzca.
