# Validación histórica de apps

[English](APP_VALIDATION.md) · [Español](APP_VALIDATION.es.md)

Este resumen reúne los resultados de las versiones indicadas. El
[registro original](APP_VALIDATION.md) conserva las pruebas, imágenes, hashes,
direcciones y límites de cada captura. Las direcciones y atajos antiguos no son
instrucciones para instalar el conjunto actual.

## Dungeon — 2026-10-01

Pasaron las pruebas, los previews y la compilación ESP-IDF 5.5.1 de las
revisiones de cámara, exploración conectada y corrección del hacha. Las imágenes
se escribieron y verificaron contra flash. Una observación de 115 segundos
cubrió exploración, combate normal, jefe y progresión al segundo bioma, con HP
y MP persistentes y once muestras a 30 fps. El hacha se revisó en imágenes
generadas; no se hizo una revisión visual de la pantalla física.

## Maze y Wayfarer — 2026-10-01

Maze se instaló y verificó en la placa. Su ejecución mantuvo 40 fps, con unos
4,6–4,8 ms de renderizado y 18 ms de transferencia a pantalla. La revisión
visual usó imágenes y una secuencia generada.

Wayfarer 2.0.0 se verificó en la SKU 28596 con ESP-IDF 5.5.1. La ejecución
final mantuvo 30 fps durante más de cuatro minutos e incluyó los seis eventos
aleatorios. Los previews de resolución nativa se revisaron para arte, paleta,
instrumentos y movimiento.

## Miso — 2026-09-25 y 2026-10-03

Las comprobaciones originales cubrieron 24 horas simuladas, interacción,
alimentación, movimiento, estados inválidos y renderizado, con AddressSanitizer
y UndefinedBehaviorSanitizer. El registro incluye las capturas físicas y sus
límites, además de las revisiones posteriores de comportamiento y arte.

La última revisión amplió poses y ciclo de iluminación. Se comprobaron los
previews con el renderizador real: el GIF principal tiene 420 frames y dura
28 segundos. La imagen de 366.320 bytes se instaló en su asignación de 384 KiB
y se verificó por separado contra flash. Un arranque de 36 segundos mantuvo
30,004–30,036 fps, sin frames tardíos, con 252.468 bytes de heap libre y sin
errores de sensores. Se revisó una captura exacta del framebuffer. Esta prueba
cubrió arranque y actividad diurna temprana; no comprobó gestos físicos,
el ciclo completo de iluminación ni legibilidad a distancia de uso.

## Lumen

Una medición anterior a la limpieza del repositorio mantuvo 33 fps en tres
ventanas de cien frames, con unos 23–25 ms de dibujo y transferencia por frame
y 194 KiB de heap libre. Todas las muestras tenían datos de la IMU. Son
mediciones USB en una placa; no se midió autonomía de batería.
