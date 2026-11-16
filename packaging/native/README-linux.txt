Harvest Moon: Magical Melody - native Linux version / versión nativa para Linux
=================================================================================

ESPAÑOL
-------

Qué es
  Un port no oficial que corre Harvest Moon: Magical Melody de GameCube como un
  programa de Linux, sin emulador. Este paquete NO trae el juego: el juego se
  hace en tu computadora con tu propia copia, la imagen de tu disco.

Lo que necesitas
  - Un Linux de 64 bits con glibc 2.38 o más nueva (VERSIONS.txt dice la
    exacta): Ubuntu 24.04 o más nuevo, Debian 13, Fedora 39 o más nuevo,
    Arch, SteamOS 3.6 o más nuevo (Steam Deck), o similares. El paquete "x64"
    es para procesadores Intel y AMD; el "arm64", para ARM.
  - Una tarjeta gráfica con Vulkan (con el controlador de tu distribución,
    Mesa o el de NVIDIA).
  - La imagen de tu disco de Harvest Moon: Magical Melody de Estados Unidos
    (G4AEE9), en .iso, .gcm o .rvz (las de Dolphin).
  - Unos 2 GB libres. No hace falta instalar compilador ni nada más: el
    paquete trae el suyo.

Cómo jugar
  1. Descomprime el .tar.gz en una carpeta (con el administrador de archivos,
     o con: tar xzf HarvestMoonMM-...-linux-x64.tar.gz).
  2. Abre HarvestMoonMM (doble clic, o ./HarvestMoonMM en una terminal).
  3. Elige la imagen de tu disco (o suéltala sobre la ventana). La primera vez,
     el juego se prepara para tu computadora: tarda unos minutos y después ya
     no. Cuando el paquete se actualiza, se prepara otra vez.
  4. Pulsa Jugar.

  En la Steam Deck: en el modo escritorio, descomprime el paquete y agrega
  HarvestMoonMM a Steam como juego que no es de Steam; después ya se abre
  desde el modo juego.

  En las pestañas Opciones, Mods y Controles se eligen la imagen y el sonido,
  los mods, y se prueba el control (Q y E, o LB y RB, cambian de pestaña).
  Juega el primer control que conectes; sin control, el teclado.
  Dentro del juego, F10 (o el botón Select del control) abre las opciones sin
  salir del juego, y Tab mantenido lo acelera.

Dónde se guarda todo
  En ~/.local/share/HarvestMoonMM: tus elecciones (settings.json) y, en la
  carpeta Native, tus partidas, el juego preparado y los registros
  (prepare.log de la preparación, native.log del juego).

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
  a Linux program, without an emulator. This package does NOT include the
  game: the game is made on your computer from your own copy, your disc image.

What you need
  - A 64-bit Linux with glibc 2.38 or newer (VERSIONS.txt says the exact one):
    Ubuntu 24.04 or newer, Debian 13, Fedora 39 or newer, Arch, SteamOS 3.6 or
    newer (Steam Deck), or the like. The "x64" package is for Intel and AMD
    processors; the "arm64" one, for ARM ones.
  - A graphics card with Vulkan (with your distribution's driver, Mesa or
    NVIDIA's).
  - The image of your disc of Harvest Moon: Magical Melody for the USA
    (G4AEE9), as .iso, .gcm or .rvz (Dolphin's).
  - About 2 GB free. There is no compiler or anything else to install: the
    package brings its own.

How to play
  1. Unpack the .tar.gz into a folder (with the file manager, or with:
     tar xzf HarvestMoonMM-...-linux-x64.tar.gz).
  2. Open HarvestMoonMM (double-click, or ./HarvestMoonMM in a terminal).
  3. Choose your disc image (or drop it on the window). The first time, the
     game is prepared for your computer: it takes a few minutes, and then never
     again. When the package is updated, it is prepared again.
  4. Press Play.

  On the Steam Deck: in desktop mode, unpack the package and add HarvestMoonMM
  to Steam as a non-Steam game; from then on it opens from game mode.

  The tabs Options, Mods and Controls choose the picture and the sound, the
  mods, and let you try the controller (Q and E, or LB and RB, change the
  tab). The first controller you connect plays; without one, the keyboard
  does. In the game, F10 (or the controller's Select button) opens
  the options without leaving the game, and holding Tab speeds it up.

Where everything is kept
  In ~/.local/share/HarvestMoonMM: your choices (settings.json) and, in the
  Native folder, your saves, the prepared game and the logs (prepare.log of the
  preparation, native.log of the game).

If something fails
  The launcher says what happened and offers to open the log. With the log,
  you can ask for help at https://github.com/RodoLy/HMMMGC (Issues).

License
  GPL-3.0 (LICENSE.txt). What the package brings from other projects and their
  licenses: THIRD_PARTY_NOTICES.txt and the licenses folder. The versions it was
  made with: VERSIONS.txt.
