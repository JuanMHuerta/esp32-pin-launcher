# Lecciones del dispositivo ESP32

[English](ESP32_DEVICE_LESSONS.md) · [Español](ESP32_DEVICE_LESSONS.es.md)

Hallazgos reutilizables de hardware y sensores para Waveshare
ESP32-S3-Touch-AMOLED-1.91 (SKU 28596). Las reglas están en el
[contrato de la placa](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md); las
[fuentes originales](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.es.md)
distinguen documentación y observaciones de una sola placa.

Al agregar un hallazgo, indicá placa/revisión o alcance, síntoma, prueba o fuente
y regla práctica. Conservá la distinción entre una observación y un pinout
general. Este archivo no contiene mecánicas ni parámetros específicos de apps.

- **Alineación de particiones de app (verificada en fuentes, ESP32-S3):**
  ESP-IDF exige inicios alineados a 64 KiB. Cada imagen ocupa una partición
  contigua y puede tener distinto tamaño. Lo verifican el generador de ESP-IDF
  5.5.1 y su [documentación](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/partition-tables.html).
  Redondeá asignaciones a bloques de 64 KiB y derivá direcciones de la misma
  tabla. Si se mueven límites, reinstalá tabla e imágenes afectadas juntas:
  una dirección antigua puede escribir sobre otra app.
- **Nombre del controlador (verificado en fuentes):** el IC físico RM67162 usa
  la interfaz QSPI compatible con SH8601 del ejemplo Waveshare. Comprobá
  inicialización y transferencias antes de reemplazar por el nombre.
- **Despertar táctil (datasheet y una placa):** un FT3168 en reposo puede emitir
  NACK después del tráfico I2C a otro dispositivo. Conservá la interrupción y
  reintentá tras un toque antes de concluir que falta el sensor.
- **Propiedad de búferes DMA (regla de interfaz):** la pantalla puede seguir
  leyendo un búfer después del retorno de la función de dibujo. Reutilizalo
  sólo tras el callback de finalización para evitar corrupción de franjas.
- **SDMMC y pantalla (observación local, SKU 28596, silicio ESP32-S3 v0.2):**
  el mapeo Waveshare `VersionControl_V2` de un bit —CLK9, CMD42, D0=8— montó
  una tarjeta de 31.116.288 sectores junto con la pantalla SH8601. Se verificó
  el 2026-10-01 con el menú y pruebas recursivas de lectura, escritura y borrado
  por USB. Aplicalo sólo a la placa y ejemplo comprobados; para otra revisión,
  repetí la prueba de SD y pantalla juntas.
