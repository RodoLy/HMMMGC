# Shared / Compartido

[Español](#español) · [English](#english)

## Español

Lo que comparten los programas de la versión nativa. `shared.cmake` lo compila
dentro de un programa que ya enlaza Dear ImGui, SDL3 y nlohmann/json
(`add_shared_ui(<programa>)`).

| Archivo | Qué es |
|---|---|
| `theme.h`, `theme.cpp` | El aspecto a partir del propio juego: los colores, las letras, los botones, los paneles como cuadros de diálogo y los controles de las opciones (interruptores, elecciones, porcentajes). |
| `fonts/` | Las letras (M PLUS Rounded 1c, libre; ver `fonts/README.md`), que van dentro del programa. |
| `texts.h`, `texts.cpp` | Todo lo que dicen, en español y en inglés. |
| `settings.h`, `settings.cpp` | Las elecciones del jugador en `settings.json`. |
| `option_rows.h`, `option_rows.cpp` | Las filas de las opciones (resolución, pantalla completa, pantalla ancha, cuadros por segundo, zoom, velocidad del juego, volumen), con su nombre, su pista y su control. |
| `controls.h`, `controls.cpp` | Los controles de la PC, cómo los asigna el juego, un control de GameCube dibujado y la pantalla de controles. |

## English

What the programs of the native version share. `shared.cmake` compiles it into
a program that already links Dear ImGui, SDL3 and nlohmann/json
(`add_shared_ui(<program>)`).

| File | What it is |
|---|---|
| `theme.h`, `theme.cpp` | The look after the game's own: the colors, the lettering, the buttons, the panels like dialogue boxes and the controls of the options (switches, choices, percentages). |
| `fonts/` | The lettering (M PLUS Rounded 1c, free; see `fonts/README.md`), which goes inside the program. |
| `texts.h`, `texts.cpp` | Everything they say, in Spanish and in English. |
| `settings.h`, `settings.cpp` | The player's choices in `settings.json`. |
| `option_rows.h`, `option_rows.cpp` | The rows of the options (resolution, full screen, widescreen, frames per second, zoom, game speed, volume), with their name, hint and control. |
| `controls.h`, `controls.cpp` | The PC's controllers, how the game maps them, a GameCube controller drawn, and the controls screen. |
