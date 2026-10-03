# DUNGEON//SEED

[English](README.md) · [Español](README.es.md)

Una animación autónoma de mazmorras en primera persona para la Waveshare
ESP32-S3-Touch-AMOLED-1.91 de 536 × 240. Monstruos, armas en mano y un mundo que
ocupa toda la altura están pensados para verse a distancia.

Un mapa conectado de 19 × 19 contiene nueve salas persistentes, pasillos,
ramificaciones y circuitos. La cámara camina, gira, mira atrás y revisita zonas
despejadas. Paredes, piso, techo, objetos y sprites comparten cámara y coordenadas.
Ocho destinos llevan al jefe. Los biomas alternan catacumbas frías, ruinas con
musgo, basalto ardiente y una bóveda helada. Doce tipos de sala incluyen puertas,
tesoros, santuarios, escaleras, cruces, criptas, bibliotecas, armerías y hongos.

Ocho enemigos y seis jefes tienen cuatro poses, incluidos los restos derrotados.
El combate incluye acercamiento, aviso, ataque, contraataque y derrota.
Seis armas —espada, bastón, hacha, maza, daga y ballesta— muestran manos,
preparación, golpe o proyectil y recuperación. Todas entran desde abajo a la
derecha y apuntan hacia el centro. Las transiciones cubren la entrada y las
escaleras; las salas se conectan sin cortinas. Botín abierto y restos persisten.
Los indicadores inferiores dejan margen seguro sin cortar el piso con una
franja negra.

La vida enemiga es estado de simulación: los enemigos normales requieren al
menos tres golpes y los jefes siete. La barra muestra la vida restante real.
HP y MP del jugador persisten entre encuentros, movimiento y cambios de bioma.
Los golpes acertados reducen HP; las rondas bloqueadas no. El bastón consume
ocho MP; al agotarse cambia a acero. Las pociones se gastan con una animación
de bebida; cada santuario restaura recursos una vez por mapa. La derrota del
jugador produce un fundido y reinicio explícitos.

El renderizador no asigna memoria durante el dibujo. Usa un lienzo RGB565 de
268 × 120 escalado 2×, doble búfer DMA en franjas de 40 filas y una inversión
de bytes RGB565. El framebuffer ocupa 64.320 bytes; 116 sprites indexados de
64 × 64 ocupan 475.136 bytes de flash. No requiere PSRAM. El objetivo es 30 fps;
cada diez segundos informa rendimiento, movimiento, recursos y daño por serie.
Mantené BOOT 1,5 segundos y soltalo para volver al menú.

![Combate autónomo](preview-combat.gif)

## Arte y vistas

- [Escenas seleccionadas](preview-showcase.png)
- [185 cuadros de salas, encuentros, equipo y transiciones](preview.png)
- [Cuatro biomas y seis vistas](preview-biomes.png)
- [Seis armas en cuatro momentos](preview-weapons.png)
- [Hacha en cuatro momentos](preview-axe.png)
- [192 segundos de exploración](preview-motion.gif)
- [Cronología](preview-run.png)
- [Vida persistente durante el combate](preview-combat.png)
- [Fuentes y reconstrucción del arte](assets/ART_DIRECTION.es.md)

Los PNG editables con paleta fija están en `assets/source`; los originales
generados de personajes y armas están en `assets/masters`. El importador
rechaza sprites faltantes o pegados, conserva la escala por fila y fija la
paleta. Arquitectura, materiales y efectos se editan en `tools/make_source_assets.py`.

Desde `firmwares/dungeon-pin/`:

```sh
python3 tools/make_source_assets.py
python3 tools/import_creatures.py assets/masters/monsters.png 4 assets/source/actors.png
python3 tools/import_creatures.py assets/masters/monsters-extra.png 4 assets/source/actors-extra.png
python3 tools/import_creatures.py assets/masters/bosses.png 3 assets/source/bosses.png
python3 tools/import_creatures.py assets/masters/bosses-extra.png 3 assets/source/bosses-extra.png
python3 tools/import_creatures.py assets/masters/gear-right.png 2 assets/source/gear-right.png --columns 2
python3 tools/import_creatures.py assets/masters/gear-extra-right.png 4 assets/source/gear-extra-right.png --columns 2
python3 tools/import_creatures.py assets/masters/axe-right.png 1 assets/source/axe-right.png --columns 2
python3 tools/compile_assets.py
tools/test.sh
python3 tools/make_preview.py
```

Las pruebas cubren 40 minutos simulados, caminos transitables, giros y revisitas,
todos los biomas y actores, tiempo restante, vida persistente, combates,
pociones, santuarios, proyección, indicadores, 25.920 casos de dibujo con
protecciones de memoria, todos los píxeles DMA, paletas, agarres y recursos
C reproducibles.

## Compilar e instalar

Leé el contrato de la placa y activá ESP-IDF 5.5.x. Desde esta app:

```sh
idf.py build
idf.py size
```

En la colección completa, Dungeon ocupa `ota_4`; el atajo USB es `5`.
El [script raíz](../../build-and-flash.sh) calcula dirección y capacidad en
`partitions.csv`; reinstalá todas las imágenes si cambia la distribución.
Una selección web reasigna subtipos OTA. `idf.py flash` de la app independiente
reemplaza la tabla de la colección. Las vistas comprueban composición y movimiento;
la pantalla física se comprueba mirando la placa.

Las pruebas históricas están en [validación de apps](../../documentation/APP_VALIDATION.md).
