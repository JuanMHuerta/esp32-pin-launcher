# Contrato de la placa: Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596)

[English](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md) · [Español](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md)

Validado: 2026-10-01. Alcance: **SKU 28596**, ESP-IDF. Consultá el
[registro de fuentes](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.es.md) cuando
haya una discrepancia, cambien versiones o se trabaje con SD o controladores.

## Reglas

- Target `esp32s3`, MCU ESP32-S3R8, 16 MB de flash, 8 MB de PSRAM. La pantalla
  no requiere PSRAM.
- El controlador físico es **RM67162**. Usá la interfaz QSPI compatible con
  **SH8601** del ejemplo Waveshare; el nombre del componente no justifica
  reemplazar una configuración `esp_lcd_sh8601` que funciona.
- Orientación de trabajo: **536 × 240 horizontal**, RGB565. La resolución
  nativa especificada es 240 × 536.
- Partí de los pines y la secuencia QSPI de Waveshare. Cambiá una variable
  de hardware por vez.
- Un búfer enviado a pantalla pertenece a DMA hasta el callback de finalización.
- El tacto se lee por interrupción. Un NACK I2C inicial o en reposo no prueba
  que falte el FT3168.
- Para SD, comprobá revisión de placa y ejemplo correspondiente; no generalices
  mapeos antiguos.
- No cambies versiones principales de ESP-IDF o componentes al depurar hardware.

## Mapeo de referencia

| Función | Valor |
| --- | --- |
| Host LCD | `SPI2_HOST` |
| LCD CS | GPIO6 |
| LCD CLK/PCLK | GPIO47 |
| LCD D0/D1/D2/D3 | GPIO18 / 7 / 48 / 5 |
| LCD reset | GPIO17 |
| Pantalla lógica | `536x240` |
| Táctil | FT3168, I2C `0x38` |
| I2C SCL/SDA | GPIO39 / GPIO40 |
| Touch INT | GPIO41 |
| Touch reset | Reset de la placa, sin GPIO independiente en el ejemplo estándar |
| IMU | QMI8658C en el mismo bus; probar `0x6B` y después `0x6A` |
| ADC batería | GPIO1 = `ADC1_CH0`; divisor 100k/100k, `Vbat ≈ 2*Vadc` calibrado |
| BOOT | GPIO0 |
| USB nativo | D- GPIO19, D+ GPIO20 |
| Redes microSD del esquema | MISO GPIO8, CS GPIO9, MOSI GPIO42, CLK GPIO47 |

GPIO47 también es PCLK de pantalla; el esquema comparte GPIO8/9 con SDO/TE del
panel. Evitá que controladores independientes tomen esos pines sin comprobar
la revisión y el diseño o ejemplo actual de Waveshare.

## Pantalla

Conservá primero las instrucciones de inicialización de Waveshare:

```text
orientación:     0x36 <- 0xF0
formato:         0x3A <- 0x55      # RGB565
ventana X:       0x2A <- 0..535
ventana Y:       0x2B <- 0..239
brillo:          0x51
encender panel:  0x29
```

Dependencias de la base probada:

```yaml
dependencies:
  idf: ">=5.5,<6.0"
  espressif/esp_lcd_sh8601: "2.0.1~1"
```

Reproducí con ESP-IDF 5.5.x antes de migrar. SH8601 2.0.x admite IDF 6.0, pero
esa migración es un trabajo separado. El ejemplo LVGL actual declara
`idf >5.0.4, !=5.1.1`; ese rango no prueba todas las versiones en este proyecto.

- Preferí franjas o doble búfer en memoria interna `MALLOC_CAP_DMA`.
- En rectángulos LVGL/controlador, redondeá inicios a par hacia abajo y finales
  inclusivos a impar hacia arriba; los finales exclusivos de `draw_bitmap`
  quedan pares.
- Reutilizá un búfer sólo después del callback de finalización.
- El primer cuadro debe cubrir toda la pantalla si luego se usan regiones sucias.
- Aplicá exactamente una conversión de bytes RGB565 cuando la cadena lo requiera.
  El renderizador propio probado usa `__builtin_bswap16`; LVGL de Waveshare
  entrega su búfer configurado directamente. Comprobá negro, rojo, verde,
  azul y blanco antes de combinar conversiones.
- Para una pantalla vacía: pines → reset e instrucciones → controlador QSPI →
  orientación y ventana → finalización de transferencia → renderizador.

## Tacto

En el ejemplo LVGL Waveshare, activá `EXAMPLE_USE_TOUCH=1` para SKU 28596;
el valor por defecto es `0`. Usa I2C0, SCL39, SDA40, 300 kHz en el ejemplo,
dirección `0x38` e INT41. FT3168 admite hasta 400 kHz.

Instalá la interrupción de flanco descendente de GPIO41, despertá una tarea y
leé el informe I2C. Agrupá informes del mismo contacto y reconocé toque,
arrastre y liberación en la app. Monitor/Sleep puede perder respuesta I2C
después de tráfico a otro dispositivo del bus; conservá la interrupción y
reintentá tras un contacto.

En horizontal, el primer par corresponde a Y y el segundo a X; Y se invierte.
El proyecto usa `x=clamp(raw_second,0,535)` y
`y=239-clamp(raw_first,0,239)`. Waveshare usa `240-y` con otro tratamiento de
bordes. Conservá una única convención y probá las cuatro esquinas:
`x=0..535`, `y=0..239`.

Diagnóstico: bus y pull-ups → dirección y estado de despertar → interrupción
GPIO41 → validez del informe → transformación → debounce.

## IMU

QMI8658C comparte GPIO39/40. SA0 elige `0x6A` o `0x6B`; el proyecto funcionó
en `0x6B`. Probá ambas. WHO_AM_I esperado para esta base: `0x05`.
Configurá antes de usar muestras; para gestos basados en gravedad, tomá muestras
frescas iniciales que establezcan una posición neutra. Separá datos inválidos
de errores de transacción I2C.

## SD

Redes SPI del esquema: MISO8, CS9, MOSI42, CLK47. No tomes un mapeo antiguo
`VersionControl_V2` con CLK9 como universal. Antes de trabajar con SD:

1. Identificá revisión de placa y repositorio.
2. Consultá el ejemplo Waveshare `SPI_SD` de esa revisión.
3. Resolvé la propiedad de GPIO47, compartido con pantalla.
4. Comprobá SD y pantalla juntas en hardware antes de integrar.

La observación local de SDMMC de un bit con CLK9/CMD42/D0=8 está registrada
en el ledger; se limita a la placa probada.

## Compilar, instalar y observar

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p "$PORT" flash
idf.py -p "$PORT" monitor
```

Para conservar el estado al conectar: `idf.py -p "$PORT" monitor --no-reset`.
USB puede cambiar de nombre o permisos tras reiniciar; redescubrí el puerto.
Un fallo de permisos es un problema de la computadora hasta comprobar otra
causa. Usá grupos serie o reglas udev duraderas, sin usuarios específicos.
Para el cargador ROM: mantené BOOT, presioná y soltá RESET, soltá BOOT.
El monitor normal reinicia la placa al conectarse.

## Orden de puesta en marcha

Detenete en la primera etapa que falle:

1. Firmware mínimo: arranque y registros estables.
2. Pantalla: pines e inicialización exactos, colores sólidos.
3. Bytes RGB565, orientación 536 × 240 y alineación de rectángulos.
4. Bus I2C e inicialización QMI8658C.
5. Interrupción táctil, despertar, cuatro esquinas y contactos.
6. ADC de batería.
7. SD con ejemplo correspondiente a la revisión.
8. Wi-Fi/BLE y estructura de la app.
9. Optimización de búferes y regiones sólo después de comprobar corrección.

Para cada cambio de hardware: indicá el invariante modificado, comprobá la fuente
en el ledger, cambiá una variable, compilá con herramientas fijadas, ejecutá el
diagnóstico físico mínimo y registrá el resultado exacto antes de integrar.
Preferí esquema y repositorio actuales de Waveshare, luego documentación del
fabricante, Espressif, datasheets y finalmente observaciones locales. Una
observación local puede precisar comportamiento, pero no redefinir pines generales.
