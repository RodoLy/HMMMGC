"""Text files that come out the same on every platform.

Archivos de texto que salen igual en todas las plataformas.

Python's defaults for text files depend on the machine: on Windows `Path.write_text` uses the
ANSI code page (for example cp1252) and turns every "\\n" into "\\r\\n". The same table, report
or command would then be a different file on a Windows laptop than on Linux, and a program
that expects one byte for the end of a line (ModernGekko's automation folder, a `diff` of two
reports) would see another. These functions always say what they mean: UTF-8 and "\\n".

Los valores por defecto de Python para archivos de texto dependen de la máquina: en Windows
`Path.write_text` usa la página de códigos ANSI (por ejemplo cp1252) y convierte cada "\\n" en
"\\r\\n". La misma tabla, informe o comando sería entonces un archivo distinto en una laptop
con Windows que en Linux, y un programa que espera un solo byte para el fin de línea (la
carpeta de automatización de ModernGekko, un `diff` entre dos informes) vería otro. Estas
funciones dicen siempre lo que quieren: UTF-8 y "\\n".
"""

from pathlib import Path

ENCODING = "utf-8"
END_OF_LINE = "\n"


def read_text(path: Path) -> str:
    """The contents of a text file that this project wrote (a table, a list of cases)."""
    return path.read_text(encoding=ENCODING)


def read_program_output(path: Path) -> str:
    """The contents of a file that another program wrote (a log, a coverage list, a status).

    Whatever is not valid UTF-8 becomes U+FFFD instead of stopping the reader: a program's
    output can hold any byte, and the lines this project looks for are plain ASCII.
    """
    return path.read_text(encoding=ENCODING, errors="replace")


def write_text(path: Path, text: str) -> None:
    """Write `text` as UTF-8 with "\\n" at the end of every line, on any platform."""
    path.write_text(text, encoding=ENCODING, newline=END_OF_LINE)
