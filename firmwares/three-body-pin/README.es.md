# Three Body

[English](README.md) · [Español](README.es.md)

Tres soles luminosos se atraen en un profundo campo de estrellas. Sus trayectos
recientes brillan en ámbar, cian y rosa. La pantalla muestra la animación sin
título, rótulos ni texto permanente, para que el movimiento se aprecie desde el
otro lado de una habitación.

La secuencia recorre ocho configuraciones iniciales: **Órbita en ocho**,
**Soles caóticos**, **Visitante binario**, **Triángulo roto**, **Larga
aproximación**, **Encuentro cercano**, **Órbita eco** y **Cruce solar**. Cada una
dura hasta 65 segundos. Cambia antes si un sol se aleja más de 6,4 unidades del
centro de masa; los sistemas acotados también avanzan al cumplir el límite. La
secuencia vuelve a empezar con estelas nuevas y un zoom que se ajusta con
suavidad.

La primera configuración usa las [condiciones iniciales de
Simó](https://people.ucsc.edu/~rmont/Nbdy/NbdyB.html) para la órbita periódica
en forma de ocho. Las demás exploran masas desiguales, grupos giratorios y
encuentros cercanos. Es un estudio plano de gravedad newtoniana inspirado en
los tres soles de *El problema de los tres cuerpos*. No reproduce el clima
ficticio del libro ni modela efectos relativistas. Los cuerpos siguen las
ecuaciones gravitatorias: no hay trayectos programados, rebotes contra la
pantalla ni órbitas forzadas. Integra con Runge–Kutta de cuarto orden y adapta
los pasos a los encuentros cercanos; suaviza la fuerza en todas las órbitas
salvo la figura en ocho.

Mantené cualquier parte de la pantalla durante 0,7 segundos para mostrar u
ocultar las lecturas de masa, tiempo simulado y energía. Una pulsación breve de
**BOOT** pasa a la configuración siguiente. Mantenelo **1,5 segundos** y
soltalo para volver al menú. En el menú, el atajo USB es **8**.

La pantalla usa la inicialización horizontal QSPI de Waveshare y el
controlador compatible con SH8601. La escena RGB565 de 268 × 120 se expande a
536 × 240 con doble búfer DMA interno. El tacto usa el controlador FT3168 de la
placa. No requiere PSRAM ni SD.

Desde la raíz del repositorio:

```sh
tools/test_scenes.sh
python3 tools/preview_scene.py three-body-pin --preset 0 --seconds 14
source /path/to/esp-idf/export.sh
idf.py -C firmwares/three-body-pin build
```

Las pruebas locales comprueban el período de la figura en ocho, la conservación
del momento y la energía, las ocho configuraciones, los cambios por escape y
por tiempo, el panel táctil, los límites del framebuffer y la expansión y el
orden de bytes de pantalla. `SANITIZERS=address,undefined` habilita
comprobaciones si el equipo tiene esos entornos.

Con el menú de fábrica en pantalla, verificá las ocho configuraciones y el
estado de ejecución con:

```sh
python3 tools/check_app.py three-body-pin --seconds 540 --all-profiles
```

La prueba física comprueba el arranque, la velocidad de cuadros, la memoria y la
deriva de energía. Usá el `build-and-flash.sh` de la raíz para instalar la
colección completa; `idf.py flash` de la app instala su propia tabla de
particiones.

![Órbita en forma de ocho](preview-0.png)

Vistas animadas, generadas por el renderizador C del firmware: [órbita en ocho](preview-0.gif),
[soles caóticos](preview-1.gif), [visitante binario](preview-2.gif),
[triángulo roto](preview-3.gif), [larga aproximación](preview-4.gif),
[encuentro cercano](preview-5.gif), [órbita eco](preview-6.gif) y
[cruce solar](preview-7.gif).

Los subtipos OTA describen la colección completa. Las instalaciones web
asignan subtipos consecutivos; los atajos USB conservan la identidad de cada
app.
