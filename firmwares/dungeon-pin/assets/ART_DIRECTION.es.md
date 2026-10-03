# Arte de Dungeon

[English](ART_DIRECTION.md) · [Español](ART_DIRECTION.es.md)

El renderizador usa 268 × 120 y escala 2×. Los sprites fuente ocupan celdas de
64 × 64, con una paleta compartida de 64 colores y alfa binario. El índice cero
es transparente. Conservá esa paleta antes de compilar las hojas.

## Fuentes

`source/` contiene las hojas de `tools/compile_assets.py`:

- `actors.png`, `actors-extra.png`: enemigos normales, cuatro poses por actor.
- `bosses.png`, `bosses-extra.png`: jefes, cuatro poses por actor.
- `gear.png`, `gear-extra.png`: armas anteriores conservadas para editar.
- `gear-right.png`, `gear-extra-right.png`, `axe-right.png`: armas derechas.
- `tiles-and-props.png`, `biomes-and-props.png`, `weapons-and-effects.png`:
  arquitectura, objetos, efectos y detalles de armas.

`masters/` conserva imágenes transparentes de mayor resolución de personajes
y armas, generadas con la herramienta de imágenes de OpenAI. La arquitectura,
los objetos y los efectos nativos se dibujan en `tools/make_source_assets.py`.

Los biomas usan piedra fría, musgo, basalto ardiente y hielo azul. Las antorchas
son ámbar cálido; acero y hueso deben distinguirse de las paredes. Las armas
entran desde abajo a la derecha. Conservá agarre, proporciones y colores entre
poses de reposo y ataque.

## Reconstrucción

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
./tools/test.sh
```

El importador extrae siluetas conectadas por alfa y usa una escala por fila de
animación. Rechaza sprites pegados o faltantes. Dejá espacio entre celdas para
evitar unir poses vecinas. La pose derrotada debe tener una silueta caída clara.
Las pruebas comprueban paleta, límites, materiales, dirección de armas y C
generado reproducible.
