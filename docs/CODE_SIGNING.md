# Firma de código / Code signing

[Español](#español) · [English](#english)

## Español

### Política de firma de código

Firma de código gratuita para Windows por [SignPath.io](https://about.signpath.io),
certificado de [SignPath Foundation](https://signpath.org).

**Estado:** pendiente de que SignPath Foundation acepte el proyecto. Hasta
entonces los paquetes salen sin firma.

- **Qué se firma:** `HarvestMoonMM.exe`, el launcher de los paquetes de Windows
  (x64 y ARM64), en cada release publicado. Lo demás que trae el paquete viene
  de otros proyectos de código abierto (Python, LLVM/clang de llvm-mingw,
  nodtool, el runtime de .NET y el traductor de WiiCompiled) y va como lo
  publican ellos.
- **Cómo:** el launcher se compila en GitHub Actions desde el código de este
  repositorio ([`package-native-windows.yml`](../.github/workflows/package-native-windows.yml)),
  se manda sin firma a SignPath, y quien publica aprueba la solicitud a mano
  antes de que se firme. Nada se compila ni se firma en una computadora
  personal.
- **Equipo:**
  - Autores y revisores (committers and reviewers): [RodoLy](https://github.com/RodoLy)
  - Aprobadores (approvers): [RodoLy](https://github.com/RodoLy)
- **Privacidad:** este programa no manda ninguna información a otros sistemas
  de la red, salvo que lo pida específicamente el usuario o quien lo instala o
  lo usa. El launcher y el juego no se conectan a internet: leen el disco del
  jugador y guardan sus datos en su propia computadora.

### Cómo se activa la firma (para quien publica)

El paso de firma del workflow de Windows solo corre en un release y solo
cuando el repositorio ya tiene los datos de SignPath. Mientras no los tiene,
los paquetes salen sin firma, como siempre.

1. Pedir la firma gratuita en [signpath.org](https://signpath.org) con el
   enlace del repositorio. SignPath Foundation decide si acepta el proyecto.
2. Ya aceptado, en el sitio de SignPath:
   - conectar GitHub como sistema de compilación confiable (*Trusted Build
     System*: GitHub.com) para este repositorio;
   - el proyecto con el *slug* `HMMMGC`;
   - una política de firma (*signing policy*) con el *slug* `release-signing`;
   - una configuración de artefacto (*artifact configuration*) con el *slug*
     `launcher`:

     ```xml
     <?xml version="1.0" encoding="utf-8"?>
     <artifact-configuration xmlns="http://signpath.io/artifact-configuration/v1">
       <zip-file>
         <pe-file path="HarvestMoonMM.exe">
           <authenticode-sign/>
         </pe-file>
       </zip-file>
     </artifact-configuration>
     ```
3. En GitHub, en *Settings → Secrets and variables → Actions* del
   repositorio:
   - la variable `SIGNPATH_ORGANIZATION_ID`, con el ID de la organización en
     SignPath;
   - el secreto `SIGNPATH_API_TOKEN`, con un token de la API de SignPath.
4. En cada release, SignPath avisa que hay una solicitud de firma por cada
   arquitectura; se aprueba en su sitio. El workflow espera hasta una hora y
   después arma el paquete con el launcher firmado.

## English

### Code signing policy

Free code signing for Windows provided by [SignPath.io](https://about.signpath.io),
certificate by [SignPath Foundation](https://signpath.org).

**Status:** waiting for SignPath Foundation to accept the project. Until then
the packages come out unsigned.

- **What is signed:** `HarvestMoonMM.exe`, the launcher of the Windows packages
  (x64 and ARM64), in every published release. The rest of the package comes
  from other open source projects (Python, LLVM/clang from llvm-mingw, nodtool,
  the .NET runtime and WiiCompiled's translator) and is shipped as they publish
  it.
- **How:** the launcher is built on GitHub Actions from this repository's code
  ([`package-native-windows.yml`](../.github/workflows/package-native-windows.yml)),
  sent unsigned to SignPath, and the publisher approves the request by hand
  before it is signed. Nothing is built or signed on a personal computer.
- **Team:**
  - Committers and reviewers: [RodoLy](https://github.com/RodoLy)
  - Approvers: [RodoLy](https://github.com/RodoLy)
- **Privacy:** this program will not transfer any information to other
  networked systems unless specifically requested by the user or the person
  installing or operating it. The launcher and the game do not connect to the
  internet: they read the player's disc and keep their data on the player's
  own computer.

### Turning signing on (for the publisher)

The signing step of the Windows workflow runs only for a release, and only
once the repository has SignPath's details. Until then the packages come out
unsigned, as always.

1. Apply for free signing at [signpath.org](https://signpath.org) with the
   repository's link. SignPath Foundation decides whether to accept the project.
2. Once accepted, on SignPath's site:
   - connect GitHub as a trusted build system (*Trusted Build System*:
     GitHub.com) for this repository;
   - the project, with the slug `HMMMGC`;
   - a signing policy with the slug `release-signing`;
   - an artifact configuration with the slug `launcher` (the XML above).
3. On GitHub, in the repository's *Settings → Secrets and variables → Actions*:
   - the variable `SIGNPATH_ORGANIZATION_ID`, with the organization's ID on
     SignPath;
   - the secret `SIGNPATH_API_TOKEN`, with a SignPath API token.
4. With each release, SignPath tells that there is a signing request for each
   architecture; it is approved on its site. The workflow waits up to an hour
   and then assembles the package with the signed launcher.
