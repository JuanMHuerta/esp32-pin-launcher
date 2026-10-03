# Fluid

[English](README.md) · [Español](README.es.md)

Una simulación de agua 2D PIC/FLIP que responde a la inclinación y el movimiento.
Una cuadrícula escalonada de 38 × 18 y 573 partículas producen una imagen de
67 × 30, mostrada en mosaicos de 8 × 8 píxeles. La QMI8658C aporta aceleración y
rotación.

![Agua con inclinación simulada](preview.gif)

Mantené BOOT durante 1,5 segundos y soltalo para volver al menú. No usa tacto.

## Implementación

`main/fluid.c` implementa la integración y separación de partículas,
transferencias entre partículas y cuadrícula, proyección de presión y choques
con las paredes. `main/motion.c` filtra los sensores y separa gravedad y
aceleración lineal. Las constantes están en `main/fluid_config.h`.

El renderizador lee instantáneas inmutables. La reconstrucción de densidad
conserva la masa; la velocidad determina el color. El solver, las instantáneas y
los dos búferes DMA usan SRAM interna. Física y dibujo corren en tareas distintas
sin asignaciones de memoria durante el funcionamiento estable. Las
[notas de física](docs/physics-review.es.md) explican los bordes y las
comprobaciones físicas pendientes.

## Desarrollo

Desde la raíz:

```sh
./tools/test.sh --app fluid-pin
./tools/test.sh --app fluid-pin --soak
python3 tools/make_previews.py fluid-pin
idf.py -C firmwares/fluid-pin -D FLUID_DIAGNOSTICS=0 build
```

`--soak` incluye dos simulaciones de 30 minutos. Las pruebas cubren conservación,
desprendimiento de paredes, reposo, densidad, inclinación, traslación, sacudidas
y recuperación de estados inválidos. El GIF usa gravedad simulada.

`FLUID_DIAGNOSTICS=1` habilita telemetría. CMake recuerda la opción: especificá
el valor al cambiar de modo. `tools/capture.py` captura la salida del dispositivo.
Usá el [script raíz](../../build-and-flash.sh) para instalar la colección y el
[contrato de la placa](../../documentation/AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.es.md)
para las reglas de hardware.
