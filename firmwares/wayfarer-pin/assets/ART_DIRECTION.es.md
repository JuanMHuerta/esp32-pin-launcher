# Arte de Wayfarer

[English](ART_DIRECTION.md) · [Español](ART_DIRECTION.es.md)

La cabina de carguero reparado usa crema cálido, gris oliva y ámbar. La luz fría
del exterior recorre el marco y proyecta sombras suaves sobre la consola.
Cuatro lámparas ámbar tienen centros claros, brillo aditivo y luz que respira
suavemente. La terminal ilumina de verde los paneles cercanos. El sombreado
multiplicativo conserva rayas y contraste. El exterior usa pizarra, azul oscuro
y tierra, con bordes iluminados selectivos. Las naves tienen pintura gastada,
paneles, rejillas y marcas naranja y verde azulado. Los mundos muestran costa,
nubes, hielo, erosión, tormentas, cráteres y pequeños detalles luminosos.

## Fuentes editables

En `source/`:

- `masters/cockpit-generated-v2.png`: cabina más clara; se conserva
  `cockpit-generated.png` para comparar.
- `masters/cockpit-window-mask.png`: máscara alfa de las aberturas del ventanal.
- `space-sprites-pixel.png`: tres naves, planeta, luna y asteroide en un atlas
  de tres columnas y dos filas.
- `space-pixel.png`: campo de polvo de nebulosa con desplazamiento.
- `exterior/world_0.png` a `world_5.png`: mundos oceánico, desértico, helado,
  gigante con anillos, volcánico y con cráteres; incluyen atmósfera y noche.
- `exterior/shuttle.png`, `tug.png`, `station.png`: naves pequeñas y hábitat
  anular con paneles solares, ventanas y balizas.
- `exterior/ship_freighter.png`, `ship_courier.png`, `ship_tanker.png`: cascos
  derivados del atlas original, con recortes, sombra, maquinaria y desgaste.

La cabina se reduce a 268 × 120 y se escala por vecino más cercano para conservar
bloques de 2 × 2 en 536 × 240. Los recortes se ajustan por alfa, cuantizan y
empaquetan en RGB565. El campo espacial tiene 48 colores y 17 paletas de luz.
Los exteriores usan 63 colores RGB565 más el índice cero transparente.
Los mundos ocupan lienzos de 96 × 88. Las cinco naves conservan su detalle nativo
y se escalan por vecino más cercano. El generador Python determinista produce
los PNG iniciales; se pueden editar y compilar directamente. El compilador
recorta márgenes transparentes en flash, conservando los lienzos editables.

## Preparación

Desde `firmwares/wayfarer-pin/`:

```sh
python3 tools/prepare_background.py assets/source/masters/cockpit-generated-v2.png main/assets/cockpit.rgb565 preview-cockpit.png
python3 tools/compile_assets.py
python3 tools/make_exterior_assets.py
python3 tools/make_exterior_assets.py --compile-only
```

El último comando compila originales exteriores editados sin regenerar el dibujo.

El renderizador agrega escapes, luces, instrumentos, brillo de lámparas y
pantallas, reflejos y sombras. Interpola una pequeña tabla seno y sombrea cada
mosaico lógico coherentemente entre franjas DMA. Las estrellas usan distintas
profundidades y temperaturas; las cercanas forman estelas al acelerar.
Los cambios de sector y nebulosas tiñen cielo y cabina juntos. Estaciones,
convoyes, cometas, eclipses y asteroides tienen movimiento y luces propios.

Los cometas tienen polvo curvo que se desvanece, partículas y núcleo texturado.
El eclipse usa un sol procedural de 128 × 128 en caché, granulación, bordes
atenuados, corona y una pequeña llamarada. Una luna con cráteres cruza en
diagonal; la corona queda visible durante la totalidad y baja la luz de cabina.
La luz ambiente tenue conserva las lámparas cálidas y detalles exteriores.

Los objetos mantienen posiciones 3D entre encuentros. El avance de cámara
proyecta distancia en tamaño y movimiento radial; los cercanos ocultan a los
lejanos. Naves y escapes rotan con la trayectoria por muestreo de vecino más
cercano. Los planetas llegan por distintos rumbos; los saltos aceleran sobre el
tráfico y paisaje existentes antes de revelar otro destino.
