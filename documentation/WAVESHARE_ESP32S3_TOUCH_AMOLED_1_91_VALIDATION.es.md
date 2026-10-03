# Fuentes de validación: Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596)

[English](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md) · [Español](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.es.md)

Validado: 2026-10-01. Este resumen conserva las fuentes y conclusiones de la
revisión. El [registro original](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md)
incluye el detalle de cada discrepancia. Para el trabajo habitual, usá el
[contrato de la placa](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md).

Orden de autoridad: esquema y repositorio actuales de Waveshare, documentación
del fabricante, Espressif, datasheets y observaciones locales.

| Tema | Resultado | Fuente |
| --- | --- | --- |
| SKU 28596 | Con tacto, sin header | [Waveshare](https://docs.waveshare.com/ESP32-S3-AMOLED-1.91) |
| Hardware | ESP32-S3R8, 16 MB flash, 8 MB PSRAM, 240 × 536, RM67162, FT3168, QMI8658 | [Producto](https://www.waveshare.com/esp32-s3-amoled-1.91.htm) |
| RM67162 y SH8601 | IC físico RM67162, interfaz QSPI SH8601 compatible | [FAQ](https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/FAQ) |
| Recursos | Esquema, datasheets y repositorio | [Documentos](https://docs.waveshare.com/ESP32-S3-AMOLED-1.91/Resources-And-Documents), [repositorio](https://github.com/waveshareteam/ESP32-S3-AMOLED-1.91) |
| QSPI y RGB565 | CS6, CLK47, D0/1/2/3=18/7/48/5, RST17, horizontal 536 × 240, `0x36=F0`, `0x3A=55` | [Ejemplo LVGL](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/main/example_qspi_with_ram.c) |
| Rectángulos | Inicios pares, finales inclusivos impares | [Controlador](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.1~1/readme?language=en) y ejemplo LVGL |
| Tacto | `0x38`, primer par Y, segundo X, inversión Y | [Código táctil](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/components/esp_touch/touch_bsp.c) |
| I2C | I2C0, SCL39/SDA40, 300 kHz | [Código del bus](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/components/i2c_bsp/i2c_bsp.c) |
| IDF del ejemplo | `>5.0.4, !=5.1.1` | [Manifiesto](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-AMOLED-1.91/main/02_Example/ESP-IDF/03_LVGL_V8_Test/LVGL_Test/main/idf_component.yml) |
| SH8601 | Versión comprobada 2.0.1~1; IDF ≥5.3; 2.0.0 agregó compatibilidad con IDF 6 | [Componente](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.1~1/readme?language=en), [cambios](https://components.espressif.com/components/espressif/esp_lcd_sh8601/versions/2.0.0/changelog?language=en) |
| Pines de esquema | ADC1, tacto39/40/41, SD8/9/42/47, USB19/20, flash W25Q128 | [Esquema](https://files.waveshare.com/wiki/ESP32-S3-AMOLED-1.91/ESP32-S3-AMOLED-1.91.pdf) |
| ADC | GPIO1 = ADC1_CH0 | [Espressif ADC](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc/index.html) |
| USB ROM | D-=19, D+=20; secuencia BOOT y RESET | [Conexión serie](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/establish-serial-connection.html) |
| Monitor | Reinicia al conectar; `--no-reset` evita ese reinicio | [IDF Monitor](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-monitor.html) |
| FT3168 | Monitor/Sleep puede perder respuesta I2C tras tráfico a otro dispositivo; despertar táctil recupera el estado | [Datasheet](https://files.waveshare.com/wiki/common/DATA_SHEET_FT3168.pdf) |
| QMI8658C | SA0 selecciona `0x6A` o `0x6B` | [Datasheet](https://files.waveshare.com/wiki/common/QMI8658C.pdf) |

## Discrepancias resueltas

La pantalla anuncia 16,7 millones de colores, pero este camino ESP-IDF usa
RGB565 por su instrucción `0x3A=55`; eso no limita los modos físicos del panel.
La resolución nativa 240 × 536 y la orientación lógica 536 × 240 son distintas.
La conversión de bytes depende de la cadena de dibujo: el renderizador propio
requiere `bswap16`, mientras el ejemplo LVGL entrega el color configurado.

Proyecto y Waveshare coinciden en el orden de ejes táctiles. El proyecto usa
`239-raw_y` para conservar `0..239`; el ejemplo usa `240-y` con otro tratamiento
de bordes. Elegí una convención y probá las cuatro esquinas. Un NACK inicial
de FT3168 no demuestra ausencia: su datasheet documenta el efecto de reposo
en buses con varios dispositivos.

La base local probada es IDF 5.5.1 y SH8601 2.0.1~1. El rango del ejemplo no
equivale a pruebas de todas sus versiones; reproducí 5.5.x antes de migrar a 6.

En el esquema, SD SPI usa MISO8/CS9/MOSI42/CLK47, con reloj compartido con LCD.
Los ejemplos antiguos tienen selección de revisión. En la placa local SKU 28596,
silicio v0.2, el 2026-10-01 funcionó el ejemplo SDMMC de un bit `VersionControl_V2`
con CLK9/CMD42/D0=8: montó 31.116.288 sectores y coexistió con la pantalla.
Esta observación no reemplaza el mapeo general del esquema.

También son observaciones locales: IMU en `0x6B` con WHO_AM_I `0x05`, mejor
captura de contactos por interrupción, y dibujo con franjas sin PSRAM. La
propiedad de los búferes DMA sigue siendo una regla de interfaz, independiente
de fps medidos. Investigá asignaciones reales antes de atribuir cambios de
memoria a pantalla o sensores. No generalices puertos, usuarios, fps ni umbrales
de este proyecto como ajustes de todas las placas.
