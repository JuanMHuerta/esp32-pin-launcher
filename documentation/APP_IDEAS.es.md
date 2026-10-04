# Ideas de apps

[English](APP_IDEAS.md) · [Español](APP_IDEAS.es.md)

La colección incluye Conway, Fluid, Miso, Lumen, Dungeon, Maze, Wayfarer,
Three Body y CRT. Una app nueva debe aportar otro aspecto visual o interacción.

Posibles incorporaciones:

- **Arena:** colocar materiales por tacto e inclinarlos en una cuadrícula
  celular pequeña. Empezar con arena, agua y paredes.
- **Pinball:** inclinar para empujar una bola en un tablero procedural; tocar
  para controlar paletas. Incluir juego autónomo.
- **Jardín de circuitos:** pulsos en un grafo acotado; tocar para cambiar
  conexiones o compuertas. Dibujar estados de señal claros.
- **Mapa de transporte:** trenes en una red generada, con llegadas y estaciones
  de transferencia que se distingan sin texto pequeño.
- **Campo magnético:** colocar polos por tacto y trazar el campo con partículas.
  Limitar singularidades y separar integración y dibujo.
- **Planetario mecánico:** engranajes y cuerpos en órbita con geometría original
  y una paleta legible.

Diseñá para 536 × 240: sujeto claro, movimiento visible a distancia de un brazo,
sin red obligatoria y con actividad sin intervención. Reservá BOOT durante
1,5 segundos para volver al menú. Acotá memoria y simulación; PSRAM es opcional.

Las apps existentes tienen renderizadores portables y ejemplos de tacto e IMU.
El servicio SD pertenece al menú: una app que cargue o guarde datos necesita su
propia integración SD comprobada para la revisión. Seguí
[CONTRIBUTING.es.md](../readmes/CONTRIBUTING.es.md) para registrar una app nueva.
