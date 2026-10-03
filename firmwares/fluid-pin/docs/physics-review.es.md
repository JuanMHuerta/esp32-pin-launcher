# Revisión de física — 2026-09-28

[English](physics-review.md) · [Español](physics-review.es.md)

Estas notas explican la separación del agua en las paredes y la reconstrucción
de densidad y color. Las pruebas en la computadora cubren conservación,
desprendimiento y reposo. Las capturas del dispositivo comprueban tiempos.
Las comprobaciones físicas indicadas al final se completaron el 2026-10-03.

## Separación de paredes

Tres problemas se reforzaban en `main/fluid.c`: presión negativa junto a una
pared que retenía el líquido, separación de partículas fuera del contenedor que
anulaba velocidad dirigida hacia adentro, y muestras tangenciales cero dentro
del borde que introducían fricción. El antiguo ajuste horizontal del centroide
ocultaba parte del problema, pero no resolvía una columna lateral al enderezar
ni el desprendimiento del techo. En esa versión, una piscina en el techo se
movía sólo 0,141 celdas en 200 ms al invertir gravedad, y perdía velocidad.

La presión junto a paredes se restringe ahora a valores no negativos mediante
Gauss–Seidel proyectado. Se limita la presión acumulada: una iteración posterior
puede deshacer una corrección excesiva. El interior conserva la solución normal
de incompresibilidad. Una celda de pared a presión cero admite divergencia
positiva como aproximación de un hueco que se abre; el residuo tiene en cuenta
esa desigualdad.

Sigue el principio de contornos separables de
[Batty, Bertails y Bridson, sección 4](https://www.cs.ubc.ca/~rbridson/docs/batty-siggraph2007-variationalcoupling.pdf).
Usa el stencil existente; no implementa el acoplamiento variacional completo
ni una interfaz subcelda. Los choques eliminan sólo velocidad contra la pared;
las muestras fantasma tangenciales copian la velocidad vecina. Se eliminó el
servo horizontal y sus cinco constantes.

Orden del paso: advección, separación/choques, transferencia a cuadrícula,
guardar velocidades sin fuerzas, aplicar fuerzas, resolver presión y recoger
PIC/FLIP. Gravedad y presión de soporte actúan antes de la próxima advección.
La gravedad pasó de 11,5 a 30 celdas/s² con escala temporal 1,6: la caída libre
sobre el eje corto tarda unos 0,63 segundos reales. El amortiguamiento por paso
es 0,990. Son escalas de animación, no una reproducción de agua a escala física.

### Fila fina que quedaba adherida

La primera corrección aún interpolaba velocidad normal cero de la pared.
Una partícula a 0,34 celdas recogía 66% de ese cero y sólo 34% de velocidad
interior. Ahora, después de la proyección, las celdas de pared sin presión y
con velocidad hacia el tanque extienden esa velocidad a la muestra de borde.
La cuadrícula anterior recibe la misma extensión para conservar el cambio FLIP.
La proyección sigue usando caras impermeables y conserva la restricción para
celdas apoyadas o velocidades contra la pared. Se actualizan las esquinas y
se recorre sólo el perímetro, sin memoria ni constantes nuevas.

En una inversión desde el techo, la fila más próxima se despejó en 133 ms en
lugar de 217 ms. La regresión de cuatro paredes exige densidad de borde cero
a 167 ms y píxeles negros a 200 ms. Cinco rotaciones rápidas por sentido
despejan el techo a 200 ms y conservan la fila negra desde 250 ms. La versión
anterior todavía tenía 7,32 equivalentes de partículas en el borde a 167 ms.

## Huecos y bandas oscuras en reposo

El reparto bilineal de puntos era demasiado disperso: al parar partículas y
desvanecer estelas aparecían píxeles negros interiores. Una traza vertical de
un minuto produjo hasta 31 huecos, aunque conservaba las 573 partículas y masa.

El dibujo aplica un filtro separable `[1, 2, 1]/4` antes de umbral y persistencia,
usando el arreglo temporal existente y conservando densidad por eje. La fila
de contacto se filtra sólo tangencialmente, con muestras internas reflejadas,
para no pintar agua de nuevo sobre una pared desocupada. Si la aceleración
hacia esa pared supera 1,5 celdas/s², recupera el stencil normal reflejado para
evitar un hueco mientras el agua empuja. Al invertir fuerza vuelve al aislamiento.
Se eliminó un segundo umbral de brillo que reabría huecos tenues. Se conservan
los mosaicos nativos de 8 × 8.

`test-render` agita y asienta agua un minuto en ocho posiciones: vertical,
invertida, ambos lados, dos diagonales, pitch leve y pantalla hacia arriba.
Un rellenado desde el aire exterior detecta huecos negros en RGB565; 21.600
cuadros posteriores al asentamiento no tuvieron ninguno. También comprueba
masa en esquinas, piscinas separadas, despeje e instantáneas inmutables.

Una foto del usuario mostró otro problema: una banda azul muy oscura podía
pasar una prueba que sólo contaba huecos negros cerrados. Retenciones largas,
incluido arranque sin agitación previa, reprodujeron costuras de densidad.
Convertir densidad de marcadores en opacidad hacía parecer huecos esas variaciones.

Ahora densidad define cobertura con el umbral 0,035 y la misma persistencia;
los píxeles cubiertos usan la paleta completa de agua, con reflejos de velocidad
y las tres capas nativas. El color no depende del empaquetado de marcadores.
Se eliminaron una dimensión de la tabla de color y un arreglo temporal de
velocidad, ahorrando 11.016 bytes.

Tres retenciones de cinco minutos —arranque vertical, arranque con relación
de fuerzas 0,3, y reposo tras agitación— miden brillo RGB565 a 10 Hz después
de 15 segundos. La versión anterior alcanzaba sólo 15% del verde máximo en
el interior vertical y fallaba el mínimo de 65%. La corrección obtiene mínimos
de 79%, 69% y 73%; la variación restante viene de reflejos por velocidad.
Las pruebas de huecos, masa, separación, movimiento y paredes siguen pasando.

Caminar cambia en promedio 19% de la ocupación dibujada, con máximo 33%.
Los deslizamientos cortos comprueban desplazamiento del centroide de la forma
ocupada mayor a un píxel, además de la densidad. Las mediciones fueron 1,83
píxeles horizontal, 2,00 vertical y 1,59 con pantalla hacia arriba. Así se evita
confundir parpadeo de huecos internos con movimiento visible.

## Sensores

Se conservan estimación de gravedad, filtro lineal de 15 ms, ejes y protección
de pitch. El solver se separa con gravedad sola, sin depender de traslación.

Una investigación posterior encontró bias de giroscopio sin calibrar tras
18 minutos: un eje quieto informaba unos 13,4°/s y el filtro estimaba aceleración
lineal persistente lateral y vertical. La primera muestra de acelerómetro,
tomada durante movimiento, podía fijar una magnitud de reposo equivocada y
bloquear la calibración. Ahora ésta usa estabilidad dentro de la ventana candidata
y fija la magnitud al completarla. Una regresión empieza con un impulso de
13,5 m/s² y comprueba recuperación de calibración y estimaciones de reposo.

Las ayudas acotadas de traslación y roll hacen visible caminar. La velocidad
con fuga no mide la velocidad sostenida de la carcasa. La escala con pantalla
hacia arriba pasó de 10 a 35 celdas/m; la vertical sigue en 18 celdas/m con
la ayuda vertical existente. Las pruebas de pitch rechazan saltos espurios.
Una IMU de seis ejes no distingue de forma única toda inclinación lenta y
traslación: las trazas sintéticas prueban dirección y límites, no sensación de uso.

## Rendimiento histórico

Se conservan cuadrícula 38 × 18, 573 partículas, dos pasadas de separación y
22 de presión. El solver ocupa 43.876 bytes, 2.752 más por presión y diagnóstico.
No asigna memoria durante las actualizaciones. El contorno se recorre por sus
bordes. Recíprocos precalculados evitan 6.876 divisiones flotantes por paso en
el hash y 1.146 en PIC/FLIP; se inspeccionó el código objeto ESP32.

| Captura | Física Hz | Pantalla fps | Paso máximo | Reinicios |
| --- | ---: | ---: | ---: | ---: |
| Separación principal, 45 s quieto | 60,04 | 58,75 | 10,55 ms | 0 |
| Separación principal, 35 s de impulsos | 60,02 | 58,73 | 10,83 ms | 0 |
| Fila fina, 50 s quieto | 60,02 | 58,74 | 10,69 ms | 0 |
| Densidad en reposo, 60 s quieto | 60,03 | 57,55 | 10,70 ms | 0 |
| Color interior, 60 s quieto | 60,03 | 59,84 | 10,66 ms | 0 |

La prueba de estrés repitió veinte impulsos en dos segundos y cuatro segundos
de recuperación. Conservó partículas y masa; sensores a unos 125 Hz y cero
reinicios. Las capturas posteriores de fila, densidad y color conservaron
memoria estable; la última dejó 145.788 bytes internos libres. Son mediciones
de planificación y memoria, no una confirmación visual del agua física.

## Comprobaciones en la computadora

Las pruebas recientes midieron 96–99% de `g*dt` al separarse de cada pared,
1,33–1,35 celdas de desplazamiento en 200 ms y aceleración continua. Al enderezar
columnas laterales, la fracción superior bajó de aproximadamente 0,51 a
0,27–0,29 en 500 ms y 0,066–0,073 al segundo.

- `test-boundaries`: asentamiento en las cuatro paredes, gravedad inversa,
  impulso de 65–115% de `g*dt`, aceleración continua y desplazamiento; drenaje
  lateral, densidad y píxeles de borde tras desprendimiento y rotación rápida.
- `test-walk`: pasos de 1,7 Hz, oscilación de 3,4° y traslación de 0,08 g por eje;
  ocupación dibujada, velocidad y recuperación.
- Inclinación, pitch, filtro, deslizamientos, sacudidas, rotación, conservación,
  recuperación de estados inválidos y franjas del renderizador.
- Dos simulaciones de 30 minutos con traslación y fuerzas fuertes.
- Instrumentación de comportamiento indefinido en modo trap; en esta revisión
  histórica faltaba la biblioteca AddressSanitizer. Consultá el
  [registro actual](../../../documentation/REPOSITORY_VALIDATION.es.md) para pruebas posteriores.
- Compilación ESP-IDF y tamaño de imagen.

Los límites actuales de desprendimiento parten de una piscina asentada y usan
aceleración. Reemplazan las antiguas aserciones que necesitaban el salto
instantáneo artificial del centroide.

## Comprobación física — 2026-10-03

Se instaló el firmware actual, controlado por sensores, en la Waveshare
ESP32-S3-Touch-AMOLED-1.91 conectada. El usuario revisó la pantalla física y
confirmó las tres comprobaciones: el agua asentada no tenía huecos oscuros ni
una franja en V, se desprendía rápidamente al inclinar la placa y se movía de
forma visible al llevarla como al caminar. Esto cierra las comprobaciones
pendientes de la revisión del 2026-09-28. Es una observación en esta placa,
no una medición de todas las orientaciones posibles al llevarla puesta.

Una captura serial independiente confirmó que la instalación con sólo Fluid
ignoraba el atajo de una app omitida y reiniciaba Fluid después del intervalo
de cinco minutos de Demo.
