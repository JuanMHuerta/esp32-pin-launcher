# Validación histórica de Three Body y CRT

[English](SCENE_VALIDATION.md) · [Español](SCENE_VALIDATION.es.md)

Este resumen cubre las pruebas del 2026-10-01 y las revisiones de CRT del
2026-10-02. El [registro original](SCENE_VALIDATION.md) conserva cada medición,
hash y versión. Usaba diez apps y particiones anteriores; sus direcciones y
atajos no corresponden al menú actual.

Se usó la Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596, con silicio v0.2,
flash de 16 MB y PSRAM integrada de 8 MB. ESP-IDF era 5.5.1 y el controlador
`esp_lcd_sh8601`, 2.0.1~1. Las apps no necesitaban PSRAM.

Las pruebas de Three Body comprobaron integración, conservación de energía,
retorno de la figura en ocho, zoom finito, cambios de escenario y límites del
renderizador. Las de CRT comprobaron eventos, glifos, escritura antes de la
salida, progreso, scroll, independencia del paso temporal y límites del buffer.
Se revisaron PNG y GIF generados por los renderizadores C de las apps.

En la primera prueba física, Three Body recorrió tres escenarios durante
140 segundos, con 25,0–34,4 fps. CRT recorrió sus tres secuencias originales
durante 180 segundos, con 20,9–21,7 fps. La escritura se verificó contra flash
y los registros no mostraron panic ni fallos de transferencia. Se usaron
atajos USB; no se comprobaron pulsaciones físicas de BOOT.

Las revisiones posteriores hicieron que CRT arrancara una vez y mantuviera
scroll y color a lo largo de seis flujos continuos. Se probaron cambios de
fuente y curvatura, y se amplió el contenido hasta 817 eventos y 317,9 segundos.
Las pruebas cubrieron todos los eventos y glifos, cien ciclos completos,
límites temporales, continuidad y buffers. En esas fechas faltaban las
bibliotecas de sanitizers; las comprobaciones posteriores figuran en la
[validación del repositorio](REPOSITORY_VALIDATION.es.md).

La última observación física de ese registro cubrió dos flujos, con 20,5–21,2 fps
y heap estable de 157.904 bytes en las muestras finales. Las pruebas de la
computadora cubrieron los seis flujos. Una interrupción USB y una línea serial
incompleta limitaron los colectores; el registro original describe ambas.
No se obtuvo una nueva lección reutilizable de hardware.
