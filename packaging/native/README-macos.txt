Harvest Moon: Magical Melody - native Mac version / versión nativa para Mac
=============================================================================

ESPAÑOL
-------

Qué es
  Un port no oficial que corre Harvest Moon: Magical Melody de GameCube como
  una app de Mac, sin emulador. Este paquete NO trae el juego: el juego se
  hace en tu Mac con tu propia copia, la imagen de tu disco.

Lo que necesitas
  - macOS 12 (Monterey) o más nuevo (VERSIONS.txt dice el exacto). El paquete
    "arm64" es para las Mac con Apple Silicon (M1, M2, M3, M4...); el "x64",
    para las Mac con Intel.
  - Las herramientas de línea de comandos de Apple (Command Line Tools), con
    las que se compila el juego en tu Mac. Si no las tienes, el launcher abre su
    instalador la primera vez; también se instalan desde la Terminal con:
    xcode-select --install
  - La imagen de tu disco de Harvest Moon: Magical Melody de Estados Unidos
    (G4AEE9), en .iso, .gcm o .rvz (las de Dolphin).
  - Unos 2 GB libres.

Cómo jugar
  1. Descomprime el zip y mueve HarvestMoonMM.app a tu carpeta Aplicaciones.
  2. La app no está firmada por Apple, así que la primera vez macOS no la abre
     con doble clic. Ábrela así: clic derecho (o Control + clic) sobre la app
     → Abrir → Abrir. En macOS 15 o más nuevo, si no aparece esa opción: trata
     de abrirla una vez, luego ve a Configuración del Sistema → Privacidad y
     seguridad, y abajo pulsa "Abrir de todas formas".
  3. Elige la imagen de tu disco (o suéltala sobre la ventana). La primera vez,
     el juego se prepara para tu Mac: tarda unos minutos y después ya no.
     Cuando el paquete se actualiza, se prepara otra vez.
  4. Pulsa Jugar.

  En las pestañas Opciones, Mods y Controles se eligen la imagen y el sonido,
  los mods, y se prueba el control (Q y E, o LB y RB, cambian de pestaña).
  Juega el primer control que conectes; sin control, el teclado.
  Dentro del juego, F10 (o el botón Select del control) abre las opciones sin
  salir del juego, y Tab mantenido lo acelera.

Dónde se guarda todo
  En ~/Library/Application Support/HarvestMoonMM: tus elecciones
  (settings.json) y, en la carpeta Native, tus partidas, el juego preparado y
  los registros (prepare.log de la preparación, native.log del juego).

Si algo falla
  El launcher dice qué pasó y deja abrir el registro. Con el registro, se
  puede pedir ayuda en https://github.com/RodoLy/HMMMGC (Issues).

Licencia
  GPL-3.0 (LICENSE.txt). Lo que el paquete trae de otros proyectos y sus
  licencias: THIRD_PARTY_NOTICES.txt y la carpeta licenses. Las versiones con
  las que se hizo: VERSIONS.txt.


ENGLISH
-------

What it is
  An unofficial port that runs the GameCube's Harvest Moon: Magical Melody as
  a Mac app, without an emulator. This package does NOT include the game: the
  game is made on your Mac from your own copy, your disc image.

What you need
  - macOS 12 (Monterey) or newer (VERSIONS.txt says the exact one). The
    "arm64" package is for Macs with Apple silicon (M1, M2, M3, M4...); the
    "x64" one, for Macs with Intel.
  - Apple's Command Line Tools, with which the game is compiled on your Mac.
    If you don't have them, the launcher opens their installer the first time;
    they also install from the Terminal with: xcode-select --install
  - The image of your disc of Harvest Moon: Magical Melody for the USA
    (G4AEE9), as .iso, .gcm or .rvz (Dolphin's).
  - About 2 GB free.

How to play
  1. Unzip it and move HarvestMoonMM.app to your Applications folder.
  2. The app is not signed by Apple, so the first time macOS does not open it
     with a double-click. Open it this way: right-click (or Control-click) the
     app → Open → Open. On macOS 15 or newer, if that choice is not there: try
     to open it once, then go to System Settings → Privacy & Security and,
     at the bottom, press "Open Anyway".
  3. Choose your disc image (or drop it on the window). The first time, the
     game is prepared for your Mac: it takes a few minutes, and then never
     again. When the package is updated, it is prepared again.
  4. Press Play.

  The tabs Options, Mods and Controls choose the picture and the sound, the
  mods, and let you try the controller (Q and E, or LB and RB, change the
  tab). The first controller you connect plays; without one, the keyboard
  does. In the game, F10 (or the controller's Select button) opens
  the options without leaving the game, and holding Tab speeds it up.

Where everything is kept
  In ~/Library/Application Support/HarvestMoonMM: your choices (settings.json)
  and, in the Native folder, your saves, the prepared game and the logs
  (prepare.log of the preparation, native.log of the game).

If something fails
  The launcher says what happened and offers to open the log. With the log,
  you can ask for help at https://github.com/RodoLy/HMMMGC (Issues).

License
  GPL-3.0 (LICENSE.txt). What the package brings from other projects and their
  licenses: THIRD_PARTY_NOTICES.txt and the licenses folder. The versions it was
  made with: VERSIONS.txt.
