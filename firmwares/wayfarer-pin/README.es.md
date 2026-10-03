# Wayfarer

[English](README.md) · [Español](README.es.md)

Una cabina de nave espacial en pixel art con paneles, mandos, desgaste y pequeños
detalles naranja y verde azulado. La luz del cielo se refleja en la cabina
mientras las estrellas titilan o se estiran al acelerar.

![Cabina y mundos durante el viaje](preview.gif)

Seis mundos pasan a distintas distancias: oceánico, desértico, helado, gigante
con anillos, volcánico y luna con cráteres. Tienen lados nocturnos oscuros,
terreno, nubes, fracturas y luces pequeñas. Cinco diseños de naves forman
tráfico y convoyes con motores y luces intermitentes. El ciclo también incluye
estaciones orbitales, cometas, eclipses, asteroides y nebulosas. Los eclipses
muestran un sol texturado, una luna con cráteres y pequeños detalles de corona;
durante la totalidad baja la luz de la cabina.

Cada viaje tiene llegada, encuentro local y salida. Los mundos empiezan pequeños,
crecen al acercarse y salen del ventanal por distintos rumbos. Las naves vuelan
en diagonal con casco y escape alineados. Los objetos persisten entre encuentros
y se ordenan por distancia: el tráfico cercano puede pasar delante de un mundo.
La aceleración del salto deja atrás el escenario anterior antes de revelar otro
destino. Los encuentros locales siguen un ciclo barajado.

Otras vistas:

- [Viaje continuo](preview-journey.gif) y [cuadros del viaje](preview-journey-frames.png)
- [Eclipse](preview-eclipse.gif)
- [Recursos exteriores](preview-exterior-assets.png)
- [Seis mundos y sus lados nocturnos](preview-worlds.png)
- [Escena fija](preview-scene.png) e [iluminación](preview-lighting.png)

Mantené BOOT 1,5 segundos y soltalo para volver al menú. El atajo USB es `7`.
En la colección completa ocupa `ota_6`; una selección web reasigna los subtipos
OTA consecutivamente.

## Desarrollo

Las [notas de recursos](assets/ART_DIRECTION.es.md), describen el arte
y su reconstrucción. Desde `firmwares/wayfarer-pin/`:

```sh
python3 tools/prepare_background.py assets/source/masters/cockpit-generated-v2.png main/assets/cockpit.rgb565 preview-cockpit.png
python3 tools/compile_assets.py
python3 tools/make_exterior_assets.py
```

Para compilar PNG exteriores editados sin regenerar el dibujo, usá
`python3 tools/make_exterior_assets.py --compile-only`.

Las vistas usan el renderizador C de la escena:

```sh
python3 tools/make_preview.py --seconds 8 --fps 12
python3 tools/make_preview.py --showcase --seed 25 --seconds 96 --fps 10 --output preview-journey.gif --still preview-journey.png
python3 tools/make_preview.py --event 10 --seed 25 --seconds 22 --fps 12 --output preview-eclipse.gif --still preview-eclipse.png
bash tools/test.sh
```

Desde la raíz, `./tools/test.sh --app wayfarer-pin` prueba la escena e
`idf.py -C firmwares/wayfarer-pin build` compila con ESP-IDF activado.
Instalá mediante el [script raíz](../../build-and-flash.sh) para conservar una
tabla y colección coherentes.
