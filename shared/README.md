# Shared / Compartido

[Español](#español) · [English](#english)

## Español

Lo que comparten los programas de la versión nativa: el launcher y el menú de
opciones dentro del juego (`runtime/src/in_game_menu.h`), que hablan, guardan y
leen los controles igual. El launcher tiene además su propio aspecto, el de su
diseño (`launcher/src/look.h`); el aspecto de aquí es el del menú de opciones.
`shared.cmake` lo compila dentro de un programa que ya enlaza Dear ImGui, SDL3 y
nlohmann/json (`add_shared_ui(<programa>)`, y `embed_file_as_array` mete un
archivo, como una fuente, dentro del programa); en el juego, ImGui es el de
aurora, que lo dibuja sobre cada cuadro.

| Archivo | Qué es |
|---|---|
| `theme.h`, `theme.cpp` | El aspecto del menú de opciones, a partir del propio juego: los colores, las letras, los botones, los paneles como cuadros de diálogo y los controles de las opciones (interruptores, elecciones, porcentajes). |
| `fonts/` | Las letras del menú de opciones (M PLUS Rounded 1c, libre; ver `fonts/README.md`), que van dentro del programa. |
| `texts.h`, `texts.cpp` | Todo lo que dicen, en español y en inglés. |
| `settings.h`, `settings.cpp` | Las elecciones del jugador en `settings.json`. |
| `option_rows.h`, `option_rows.cpp` | Las filas de las opciones (resolución, pantalla completa, pantalla ancha, cuadros por segundo, zoom, velocidad del juego, volumen), con su nombre, su pista y su control. |
| `controls.h`, `controls.cpp` | Los controles de la PC, cómo los asigna el juego, un control de GameCube dibujado y la pantalla de controles. |

## English

What the programs of the native version share: the launcher and the options
menu inside the game (`runtime/src/in_game_menu.h`), which speak, save and read
the controllers the same. The launcher also has a look of its own, its design's
(`launcher/src/look.h`); the look here is the options menu's. `shared.cmake`
compiles it into a program that already links Dear ImGui, SDL3 and
nlohmann/json (`add_shared_ui(<program>)`, and `embed_file_as_array` puts a
file, such as a font, inside the program); in the game, ImGui is aurora's,
which draws it over each frame.

| File | What it is |
|---|---|
| `theme.h`, `theme.cpp` | The options menu's look, after the game's own: the colors, the lettering, the buttons, the panels like dialogue boxes and the controls of the options (switches, choices, percentages). |
| `fonts/` | The options menu's lettering (M PLUS Rounded 1c, free; see `fonts/README.md`), which goes inside the program. |
| `texts.h`, `texts.cpp` | Everything they say, in Spanish and in English. |
| `settings.h`, `settings.cpp` | The player's choices in `settings.json`. |
| `option_rows.h`, `option_rows.cpp` | The rows of the options (resolution, full screen, widescreen, frames per second, zoom, volume), with their name, hint and control. |
| `controls.h`, `controls.cpp` | The PC's controllers, how the game maps them, a GameCube controller drawn, and the controls screen. |
