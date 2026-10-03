# Three Body / Gravity Lab

[English](README.md) · [Español](README.es.md)

Tres soles luminosos se atraen en una pantalla de observatorio oscura. Las
estelas ámbar, cian y carmesí muestran sus trayectorias recientes. La cámara
ajusta suavemente el zoom; el panel lateral informa masa, tiempo simulado,
energía total y deriva relativa de energía.

![Órbita en forma de ocho](preview-0.gif)

La app cambia de escenario cada 45 segundos:

- **Figure eight:** tres masas iguales siguen una órbita periódica con las
  [condiciones de Simó](https://people.ucsc.edu/~rmont/Nbdy/NbdyB.html), gravedad
  newtoniana, G=1 y sin suavizado.
- **Chaotic suns:** masas distintas intercambian energía en encuentros cercanos.
- **Binary visitor:** un tercer sol liviano se acerca a una pareja en órbita.

Los últimos dos usan suavizado de Plummer de 0,04 y 0,025 unidades para los
encuentros cercanos. Es una simulación gravitatoria plana ilustrativa inspirada
en los tres soles del libro; no reproduce su clima ficticio ni relatividad.
Integra con Runge–Kutta de cuarto orden y pasos adaptados a tiempos de caída
libre y cruce. No hay trayectorias programadas, rebotes en la pantalla ni
confinamiento artificial. Si un cuerpo escapa, la cámara aleja la vista.

Tocá BOOT para pasar al siguiente escenario. Mantenelo 1,5 segundos y soltalo
para volver al menú. El atajo USB es `8`. En la colección completa ocupa
`ota_7`; su dirección y capacidad salen de `partitions.csv`. Las selecciones
web reasignan subtipos OTA.

Usa la inicialización horizontal QSPI y el controlador compatible con SH8601.
La escena RGB565 de 268 × 120 se expande a 536 × 240 con doble búfer DMA interno.
No necesita PSRAM ni SD.

## Desarrollo

Desde la raíz:

```sh
tools/test_scenes.sh
python3 tools/preview_scene.py three-body-pin --preset 0 --seconds 14
source /path/to/esp-idf/export.sh
idf.py -C firmwares/three-body-pin build
```

Las pruebas comprueban retorno tras un período de la órbita en ocho, conservación
de energía y momento, zoom finito, cambio automático, límites del framebuffer,
expansión y bytes. `SANITIZERS=address,undefined` agrega comprobaciones si están
disponibles las bibliotecas.

Con la placa en el menú, se puede verificar arranque USB y reproducción sostenida:

```sh
python3 tools/check_app.py three-body-pin --seconds 140 --all-profiles
```

La prueba inicia la app y observa los tres escenarios; comprueba fallos, fps,
memoria y deriva de energía. Instalá con `build-and-flash.sh` de la raíz:
`idf.py flash` de la app independiente instala su propia tabla de particiones.

Más animaciones: [soles caóticos](preview-1.gif) y [visitante binario](preview-2.gif).
Se generan con el renderizador C del firmware.
