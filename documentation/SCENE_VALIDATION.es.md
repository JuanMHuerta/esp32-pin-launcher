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


## Three Body a pantalla completa — 2026-10-03

Three Body ahora ocupa toda la pantalla horizontal con un campo de estrellas,
tres soles grandes y estelas luminosas. Se quitaron el título, el nombre de la
configuración, los rótulos de los cuerpos y las instrucciones inferiores. El
panel con lecturas aparece tras mantener la pantalla 700 ms y se oculta igual.
Ocho configuraciones gravitatorias duran hasta 65 segundos; la secuencia cambia
antes si un cuerpo supera las 6,4 unidades. Una pulsación breve de BOOT sigue
adelantando la escena.

Las pruebas locales pasaron con advertencias estrictas de C. Cubren las ocho
condiciones iniciales, el período de la figura en ocho, conservación del
momento y la energía, zoom finito, cambios por escape y tiempo, mantener el
panel, el desbordamiento del reloj de 32 bits, los límites del framebuffer y la
expansión RGB565. Se regeneraron los ocho PNG y GIF con el renderizador C del
firmware. También pasaron las comprobaciones de enlaces e imágenes del
repositorio, la versión web empaquetada y sus siete pruebas de navegador. La
descripción del catálogo web está actualizada en inglés y español.

ESP-IDF 5.5.1 generó la imagen Three Body de 307872 bytes. Se instalaron el menú
y las nueve apps en la Waveshare SKU 28596, ESP32-S3 revisión v0.2. Esptool
verificó la app en `0x390000` (`ota_7`) con SHA-256
`3e4202a5f987f51200c4f7850aea9cac746b03d82f0ad8b0a76f8fc75c865c99`.
La prueba USB de 540 segundos pasó con 107 muestras que cubrieron las ocho
configuraciones, 20,7–28,1 fps y 201564–202000 bytes libres. No hubo fallos y
la deriva de energía se imprimió como `0.0000000` en todas las muestras. La
entrada IRQ del FT3168 inició correctamente; el gesto de mantener la pantalla
pasó las pruebas locales, aunque no se hizo una pulsación física durante esta
ejecución.

La inicialización de pantalla, los búferes de transferencia y la conversión
RGB565 no cambiaron. El tacto usa los pines y el despertar por interrupción ya
documentados en el contrato de placa; no se aprendió una regla nueva para la
placa.
