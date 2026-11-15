#!/usr/bin/env python3
"""Write the adoption scene of mods/marry_anyone: its script and its lines, as C arrays.

Escribe la escena de la adopción de mods/marry_anyone: su script y sus líneas, como arreglos de C.

A couple of the same sex cannot have the game's pregnancy and birth (Eve_conce and Eve_child). With
marry_anyone, when the game would start the pregnancy for such a couple, the mod counts the days of the
pregnancy as over instead, so that the next morning the game starts the birth's event; and when the game loads
that event's script and messages, the mod writes this scene over them. In it the couple meets the Harvest
Goddess (or one of her sprites) at her spring: she has seen the couple's love, and she has a baby with no
family. The couple takes the baby, the player names it and gets the birth's musical note, and the Goddess
leaves a crib at home that the player places. The game's own code for the birth (the field check's block)
keeps doing the rest: the child exists from then on as with any couple.

The script is assembled here, from what the game's scripts do (docs/SCRIPTS.md, "The adoption"). Each
built-in is called as the birth scenes call it; the comment of each step says which. The lines are new, in
the five languages of the disc, in two versions where the language needs it (two men, two women); the speaker
of each line is a code at its start (1E and the character's number), and the spouse's is written by the mod
(SPOUSE_SPEAKS). The bytes are those of the console's font (Windows-1252, with its "?" and "…").

Writes mods/marry_anyone/adoption_scene.h, which mod.c includes. tests/test_marry_anyone.py checks that the
file is up to date and that the lines fit the window.

Usage / Uso:
    python3 tools/make_adoption_scene.py
"""

import struct
import sys
import textwrap
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
OUTPUT = REPOSITORY / "mods" / "marry_anyone" / "adoption_scene.h"

# ---------------------------------------------------------------------------------------------------------
# The bytecode (tools/lsb.py describes it).

ENTER, PUSH_INT, PUSH_FLOAT, LOCAL_REF, LOCAL_GET, LOCAL_SET = 0x01, 0x02, 0x03, 0x05, 0x06, 0x07
CALL_BUILTIN_VALUE, CALL_BUILTIN, JUMP, JUMP_IF_ZERO, CALL, RETURN = 0x0B, 0x0C, 0x11, 0x12, 0x14, 0x15
ADD, SUBTRACT, EQUAL, LESS = 0x1B, 0x1C, 0x25, 0x27
HEADER_SIZE = 0x14
NO_ENTRY = 0xFFFFFFFF
BRANCH_OPERAND_BYTES = 2  # jumps and calls take a two-byte operand, so that labels can be placed later


def varint(value: int, length: int | None = None) -> bytes:
    """A signed number in groups of 7 bits, the shortest that holds it (or `length` groups)."""
    groups = 1
    while not -(1 << (7 * groups - 1)) <= value < (1 << (7 * groups - 1)):
        groups += 1
    if length is not None:
        if length < groups:
            raise ValueError(f"{value} does not fit in {length} groups")
        groups = length
    bits = value & ((1 << (7 * groups)) - 1)
    out = bytearray()
    for index in reversed(range(groups)):
        part = (bits >> (7 * index)) & 0x7F
        out.append(part | (0x80 if index else 0))
    return bytes(out)


class Assembler:
    """Builds the code of a script; labels may be used before they are placed."""

    def __init__(self) -> None:
        self.code = bytearray()
        self.labels: dict[str, int] = {}
        self.branches: list[tuple[int, str]] = []  # (offset of the opcode, label)
        self.spouse_places: list[int] = []  # code offsets of the bytes that hold the spouse's number

    def op(self, opcode: int, operand: int | None = None) -> None:
        self.code.append(opcode)
        if operand is not None:
            self.code += varint(operand)

    def push(self, *values: float | str) -> None:
        for value in values:
            if value == "spouse":
                self.code.append(PUSH_INT)
                self.spouse_places.append(len(self.code))
                self.code.append(SPOUSE_PLACEHOLDER)
            elif isinstance(value, float):
                self.code.append(PUSH_FLOAT)
                self.code += struct.pack(">f", value)
            else:
                self.op(PUSH_INT, int(value))

    def builtin(self, number: int, *arguments: float | str) -> None:
        self.push(*arguments)
        self.op(CALL_BUILTIN, number)

    def builtin_value(self, number: int, *arguments: float | str) -> None:
        self.push(*arguments)
        self.op(CALL_BUILTIN_VALUE, number)

    def branch(self, opcode: int, label: str) -> None:
        self.branches.append((len(self.code), label))
        self.code.append(opcode)
        self.code += bytes(BRANCH_OPERAND_BYTES)

    def label(self, name: str) -> None:
        self.labels[name] = len(self.code)

    def finish(self, main: str) -> bytes:
        for at, name in self.branches:
            self.code[at + 1 : at + 1 + BRANCH_OPERAND_BYTES] = varint(
                self.labels[name] - at, BRANCH_OPERAND_BYTES
            )
        size = HEADER_SIZE + len(self.code)
        header = struct.pack(">5I", NO_ENTRY, self.labels[main], NO_ENTRY, size, 0)
        return header + bytes(self.code)


# The spouse's number is written by the mod over this byte (one byte: every partner is below 64).
SPOUSE_PLACEHOLDER = 0x3F

# ---------------------------------------------------------------------------------------------------------
# The scene.

# Actors: a type (1 a character, 8 the baby) and a character number, as the birth scenes create them.
PLAYER = (1, 121)
GODDESS = (1, 47)
SPRITE = (1, 42)  # the first of the three Harvest Sprites (42 to 44)
BABY = (8, 45)
SPOUSE = (1, "spouse")
# Who brings the baby: the Harvest Goddess once she is awake (the event Eve_mega02 has been seen), and one
# of her sprites while she is still stone. Each has its own script and lines.
VISITORS = {"goddess": GODDESS, "sprite": SPRITE}

SPRING = 26  # built-in 70: the Harvest Goddess's spring, as her scenes (Eve_dream00, the wedding) go there
HOUSE = 135  # the player's house, as the birth scenes go back to it
SPRING_MUSIC = 22  # built-in 171: the music of the Goddess's dream at the spring (Eve_dream00)
HOUSE_MUSIC = 34  # the music the birth scenes play in the house
BIRTH_NOTE = 51  # the musical note the birth gives (Eve_child: CALL sub(51))
NAME_THE_BABY = (5, 0)  # built-in 126: the screen to write the baby's name
# Turns of built-in 73, in degrees, as Eve_mega03 turns its actors at the spring: 0 faces south, toward the
# camera, and 180 north. The couple stands on each side of the visitor, both facing it.
FACING_SOUTH = 0
FACING_NORTH = 180
# Places at the spring (built-in 73: x, y, z), as Eve_dream00 places the Goddess and her sprites: the
# Goddess where her statue stands, over the upper terrace, a sprite before it, and the couple on the terrace
# below, where the sprites stand in that dream.
PLACES = {
    "goddess": (7.5, 1.1, 2.75),
    "sprite": (7.5, 0.0, 3.2),
    PLAYER: (6.3, 0.0, 4.8),
    SPOUSE: (8.7, 0.0, 4.8),
}
BABY_SHOWN = (7.5, 0.0, 3.9)
BABY_HIDDEN = (7.5, -100.0, 3.9)  # out of sight, until the visitor shows the baby
# The spring's light, as Eve_mega03 sets it: 159(1, 3), and 156 for each of the spring's parts.
SPRING_LIGHT = (1, 3)
SPRING_PARTS = tuple(range(9))
# The scene's camera, as Eve_mega03 sets its own: 92(1) and 133(0, 0, 0) take the camera from the player,
# then it stands at a point (80) and looks at another (83). Here it stands to the south, a little above the
# people, and looks at the visitor: the scene is seen from the front, the couple on each side.
CAMERA_STANDS_AT = (7.5, 5.0, 20.5)
CAMERA_LOOKS_AT = (7.5, 2.9, 3.8)

# Window styles of built-in 167, as the birth scenes use them: 2 for a character of the scene.
WINDOW_OF_A_CHARACTER = 2

# The messages, by number (see LINES).
WELCOME, WATCHED, A_BABY, WOULD_YOU, SPOUSE_ACCEPTS, NAME_IT, LOVELY_NAME, SPOUSE_THANKS, A_CRIB = range(9)


def actor(who: tuple[int, int | str]) -> tuple[int | str, ...]:
    return (who[0], who[1], 0)


def say(scene: Assembler, message: int) -> None:
    scene.builtin(167, WINDOW_OF_A_CHARACTER)
    scene.builtin(51, message)  # SHOW_MESSAGE
    scene.builtin(62)  # WAIT_MESSAGE


def motion(scene: Assembler, who: tuple[int, int | str], number: int) -> None:
    scene.builtin(79, *actor(who), number)


def wait(scene: Assembler, seconds: float) -> None:
    scene.builtin(6, float(seconds))


def where_the_player_is(scene: Assembler) -> None:
    """Locals 0, 1 and 2: the player's position (built-in 116, as the birth scenes start in the house)."""
    for local in range(3):
        scene.push(0)
        scene.op(LOCAL_SET, local)
    for local in range(3):
        scene.op(LOCAL_REF, local)
    scene.builtin(116)


def end_of_a_part(scene: Assembler) -> None:
    """Fade out and close this part of the event (the birth scenes: 105(0, -127, 3000), 66, 67, 69)."""
    scene.builtin(105, 0, -127, 3000)
    wait(scene, 0.7)
    scene.builtin(66)
    scene.builtin(67)
    scene.builtin(69)
    wait(scene, 0.1)


def the_musical_note(scene: Assembler) -> None:
    """sub note(n): the birth scenes' own, without the doctor's assistant. If the player has not got note n
    (built-in 187), gives it (179), shows its sparkles and its text (63 with n + 1)."""
    scene.label("note")
    scene.op(ENTER, 6)
    scene.push(0)
    scene.op(LOCAL_GET, -2)
    scene.op(CALL_BUILTIN_VALUE, 187)
    scene.op(EQUAL)
    scene.branch(JUMP_IF_ZERO, "note_end")
    scene.op(LOCAL_GET, -2)
    scene.op(CALL_BUILTIN, 179)
    for local in range(4):
        scene.push(0)
        scene.op(LOCAL_SET, local)
    scene.push(*actor(PLAYER))
    for local in range(4):
        scene.op(LOCAL_REF, local)
    scene.builtin(93)
    scene.builtin(81, -0.2, 0.7, 0.8)
    scene.builtin(82, 8.0)
    scene.op(LOCAL_GET, 0)
    scene.op(LOCAL_GET, 1)
    scene.push(0.5)
    scene.op(LOCAL_GET, 2)
    scene.op(SUBTRACT)
    scene.op(CALL_BUILTIN, 83)
    scene.builtin(102, 1, 0)
    scene.builtin(105, 1, 0, 0)
    scene.builtin(100, 59)
    scene.push(95)
    scene.op(LOCAL_GET, 0)
    scene.push(2)
    scene.op(LOCAL_GET, 1)
    scene.op(ADD)
    scene.op(LOCAL_GET, 2)
    scene.op(CALL_BUILTIN_VALUE, 90)
    scene.op(LOCAL_SET, 4)
    for loop, effect, size in (("sparkles_in", None, 40), ("note_shows", "note", 32)):
        scene.push(0)
        scene.op(LOCAL_SET, 5)
        scene.label(loop)
        scene.push(60)
        scene.op(LOCAL_GET, 5)
        scene.op(LESS)
        scene.branch(JUMP_IF_ZERO, loop + "_end")
        if effect is None:
            scene.op(CALL_BUILTIN_VALUE, 221)
        else:
            scene.op(LOCAL_GET, -2)
            scene.op(CALL_BUILTIN_VALUE, 180)
        scene.builtin(115, size, 320, 224)
        scene.push(1)
        scene.op(LOCAL_GET, 5)
        scene.op(ADD)
        scene.op(LOCAL_SET, 5)
        scene.branch(JUMP, loop)
        scene.label(loop + "_end")
        if effect is None:
            wait(scene, 1)
    scene.builtin(167, 6)
    scene.push(1)
    scene.op(LOCAL_GET, -2)
    scene.op(ADD)
    scene.op(CALL_BUILTIN, 63)
    scene.builtin(62)
    scene.builtin(105, 1, -127, 0)
    scene.builtin(103, 1)
    scene.op(LOCAL_GET, 4)
    scene.op(CALL_BUILTIN, 91)
    scene.label("note_end")
    scene.op(RETURN, 7)


def place(scene: Assembler, who: tuple[int, int | str], where: tuple[float, float, float], turn: int) -> None:
    """Built-in 73: type, character, 0, x, y, z, turn."""
    scene.builtin(73, *actor(who), *(float(value) for value in where), turn)


def scene_script(visitor: str) -> tuple[bytes, list[int]]:
    """The script of the scene with the Goddess or the sprite, and where it holds the spouse's number."""
    guest = VISITORS[visitor]
    scene = Assembler()
    the_musical_note(scene)

    scene.label("main")
    scene.op(ENTER, 7)
    # The spring, with the player, the spouse, the visitor and the baby (70, 71 for each, 68).
    scene.builtin(70, SPRING)
    actors = (PLAYER, SPOUSE, guest, BABY)
    for who in actors:
        scene.builtin(71, *actor(who))
    scene.builtin(68)
    for who in actors:
        scene.builtin(153, *actor(who))
        scene.builtin(151, *actor(who))
    scene.builtin(159, *SPRING_LIGHT)
    for part in SPRING_PARTS:
        scene.builtin(156, part)
    place(scene, PLAYER, PLACES[PLAYER], FACING_NORTH)
    place(scene, SPOUSE, PLACES[SPOUSE], FACING_NORTH)
    place(scene, guest, PLACES[visitor], FACING_SOUTH)
    place(scene, BABY, BABY_HIDDEN, FACING_SOUTH)
    scene.builtin(92, 1)
    scene.builtin(133, 0, 0, 0)
    scene.builtin(83, *CAMERA_LOOKS_AT)
    scene.builtin(80, *CAMERA_STANDS_AT)
    scene.builtin(171, SPRING_MUSIC)
    scene.builtin(102, 0, 0)
    scene.builtin(105, 0, 0, 60)
    scene.builtin(65)
    scene.builtin(67)
    wait(scene, 1)

    scene.builtin(77, *actor(PLAYER), 1)  # "!"
    scene.builtin(77, *actor(SPOUSE), 1)
    wait(scene, 1)
    say(scene, WELCOME)
    say(scene, WATCHED)
    say(scene, A_BABY)
    # The baby comes into sight before the visitor.
    place(scene, BABY, BABY_SHOWN, FACING_SOUTH)
    motion(scene, BABY, 0)
    wait(scene, 1)
    say(scene, WOULD_YOU)
    motion(scene, SPOUSE, 6)  # happy
    scene.builtin(77, *actor(SPOUSE), 137)  # a heart
    scene.builtin(78, *actor(SPOUSE))
    say(scene, SPOUSE_ACCEPTS)
    motion(scene, SPOUSE, 0)
    say(scene, NAME_IT)
    wait(scene, 0.5)
    scene.builtin(126, *NAME_THE_BABY)
    scene.builtin(127)
    say(scene, LOVELY_NAME)
    motion(scene, PLAYER, 6)
    motion(scene, SPOUSE, 6)
    say(scene, SPOUSE_THANKS)
    motion(scene, PLAYER, 0)
    motion(scene, SPOUSE, 0)
    wait(scene, 1)

    # The musical note of the birth, here at the spring, where the player is in sight. (The birth scenes
    # give it in a room with the family; after the screen of the furniture the house is empty and the
    # camera has nothing to look at.) Then the camera of the scene again, for the visitor's last words.
    scene.push(BIRTH_NOTE)
    scene.branch(CALL, "note")
    scene.builtin(92, 1)
    scene.builtin(133, 0, 0, 0)
    scene.builtin(83, *CAMERA_LOOKS_AT)
    scene.builtin(80, *CAMERA_STANDS_AT)
    wait(scene, 0.5)
    say(scene, A_CRIB)
    end_of_a_part(scene)

    # The crib: the house, with nobody, and the screen to place it (128, 129), as the birth scenes do.
    scene.builtin(70, HOUSE)
    scene.builtin(171, HOUSE_MUSIC)
    scene.builtin(68)
    where_the_player_is(scene)
    for local in range(3):
        scene.op(LOCAL_GET, local)
    scene.op(CALL_BUILTIN, 83)
    scene.builtin(81, 0.0, 0.3, 1.5)
    scene.builtin(82, 11.0)
    wait(scene, 0.3)
    scene.builtin(102, 0, 0)
    scene.builtin(105, 0, 0, 60)
    wait(scene, 2)
    scene.builtin(65)
    scene.builtin(67)
    wait(scene, 0.1)
    scene.builtin(128)
    scene.builtin(129)
    wait(scene, 0.1)

    # The end of the event.
    scene.builtin(66)
    scene.builtin(67)
    scene.builtin(105, 0, -127, 3000)
    wait(scene, 2)
    scene.builtin(103, 0)
    scene.builtin(69)
    scene.op(RETURN, 7)
    script = scene.finish("main")
    return script, [HEADER_SIZE + place for place in scene.spouse_places]


# ---------------------------------------------------------------------------------------------------------
# The lines. "|" starts a new page; rows are wrapped to the window. {name} is the baby's name.

SPEAKS = b"\x1e"  # a message starts with this code and the number of who speaks it
SPOUSE_SPEAKS = SPEAKS + bytes([SPOUSE_PLACEHOLDER])  # the mod writes the spouse's number there
NEW_ROW = b"\x01\x04\x1d"
NEW_PAGE = b"\x01\x05\x1d"
END_OF_MESSAGE = b"\x01\x08\x1f\x1d"
BABYS_NAME = b"\x01\x40"
LONGEST_ROW = 28
ROWS_PER_PAGE = 3
NAME_LETTERS = 8
FONT_QUESTION_MARK = 0x9D
FONT_ELLIPSIS = 0x85

VISITOR_LINE, SPOUSE_LINE = "visitor", "spouse"
SPEAKERS = [VISITOR_LINE] * 9
SPEAKERS[SPOUSE_ACCEPTS] = SPOUSE_LINE
SPEAKERS[SPOUSE_THANKS] = SPOUSE_LINE

# The lines of each visitor in each language, for two men; WOMEN_CHANGES has those that differ for two
# women. The Goddess and her sprites are named as the disc names them in each language.
LINES: dict[str, dict[str, list[str]]] = {
    "goddess": {
        "e": [
            "Welcome to my spring, you two. Don't be afraid. I am the Harvest Goddess.",
            "From my spring I have seen the great love the two of you share.",
            "A little one has come into my care. A baby with no family to hold it.",
            "Your home is so full of love…|Would you raise this child as your own?",
            "A baby… for us? Yes! We'll love this child with all our hearts!",
            "Then this child is yours. Please, give your baby a name.",
            "{name}… What a lovely name.|May your family always be happy.",
            "We're a family now… Thank you, Harvest Goddess!",
            "I have left a crib in your home for the little one. Show me where it should go.",
        ],
        "s": [
            "Bienvenidos a mi manantial. No tengan miedo. Soy la Diosa Cosecha.",
            "Desde mi manantial he visto el gran amor que se tienen ustedes dos.",
            "Un pequeño ha quedado a mi cuidado. Un bebé sin una familia que lo abrace.",
            "Su hogar está tan lleno de amor…|¿Querrían criar a este bebé como su hijo?",
            "¿Un bebé… para nosotros? ¡Sí! ¡Lo querremos con todo el corazón!",
            "Entonces este bebé es suyo. Por favor, pónganle un nombre.",
            "{name}… Qué nombre tan bonito.|Que su familia sea siempre feliz.",
            "Ya somos una familia… ¡Gracias, Diosa Cosecha!",
            "He dejado una cuna en su casa para el pequeño. Díganme dónde la pongo.",
        ],
        "f": [
            "Bienvenue à ma source, vous deux. N'ayez pas peur. Je suis la déesse des récoltes.",
            "Depuis ma source, j'ai vu le grand amour qui vous unit.",
            "Un petit être m'a été confié. Un bébé sans famille pour le chérir.",
            "Votre foyer est si plein d'amour…|Voulez-vous élever cet enfant comme le vôtre ?",
            "Un bébé… pour nous ? Oui ! Nous l'aimerons de tout notre cœur !",
            "Alors cet enfant est le vôtre. Je vous en prie, donnez-lui un nom.",
            "{name}… Quel joli nom.|Que votre famille soit toujours heureuse.",
            "Nous sommes une famille, maintenant… Merci, déesse des récoltes !",
            "J'ai laissé un berceau chez vous pour le petit. Montrez-moi où le mettre.",
        ],
        "g": [
            "Willkommen an meiner Quelle, ihr beiden. Habt keine Angst. Ich bin die Erntegöttin.",
            "Von meiner Quelle aus habe ich die große Liebe gesehen, die euch verbindet.",
            "Ein Kleines ist in meine Obhut gekommen. Ein Baby ohne Familie.",
            "Euer Zuhause ist so voller Liebe …|Wollt ihr dieses Kind als euer eigenes großziehen?",
            "Ein Baby … für uns? Ja! Wir werden es von ganzem Herzen lieben!",
            "Dann ist dieses Kind eures. Bitte gebt eurem Baby einen Namen.",
            "{name} … Was für ein schöner Name.|Möge eure Familie immer glücklich sein.",
            "Jetzt sind wir eine Familie … Danke, Erntegöttin!",
            "Ich habe euch eine Wiege für das Kleine nach Hause gebracht. Zeigt mir, wo sie hin soll.",
        ],
        "i": [
            "Benvenuti alla mia sorgente. Non abbiate paura. Sono la Dea delle messi.",
            "Dalla mia sorgente ho visto il grande amore che vi unisce.",
            "Un piccolo è stato affidato a me. Un bimbo senza una famiglia che lo abbracci.",
            "La vostra casa è così piena d'amore…|Vorreste crescere questo bimbo come vostro figlio?",
            "Un bimbo… per noi? Sì! Lo ameremo con tutto il cuore!",
            "Allora questo bimbo è vostro. Vi prego, dategli un nome.",
            "{name}… Che bel nome.|Che la vostra famiglia sia sempre felice.",
            "Ora siamo una famiglia… Grazie, Dea delle messi!",
            "Ho lasciato una culla a casa vostra per il piccolo. Mostratemi dove metterla.",
        ],
    },
    "sprite": {
        "e": [
            "Hi, you two, yeah! Welcome to the Harvest Goddess's spring, yeah!",
            "The Harvest Goddess is still stone… But she always loved to see people who love "
            "each other, yeah!",
            "A baby came to the spring, yeah. A little one with no family to hold it.",
            "Your home is so full of love…|Would you raise this baby as your own, yeah?",
            "A baby… for us? Yes! We'll love this child with all our hearts!",
            "Yay! Then this baby is yours, yeah! Please give it a name!",
            "{name}! What a lovely name, yeah!|May your family always be happy, yeah!",
            "We're a family now… Thank you, little sprite!",
            "We left a crib in your home for the little one, yeah! Show me where it should go!",
        ],
        "s": [
            "¡Hola a los dos! ¡Bienvenidos al manantial de la Diosa Cosecha!",
            "La Diosa Cosecha sigue siendo de piedra… Pero a ella le encanta ver a quienes se quieren.",
            "Al manantial llegó un bebé. Un pequeño sin una familia que lo abrace.",
            "Su hogar está tan lleno de amor…|¿Querrían criar a este bebé como su hijo?",
            "¿Un bebé… para nosotros? ¡Sí! ¡Lo querremos con todo el corazón!",
            "¡Bien! ¡Entonces este bebé es suyo! ¡Pónganle un nombre!",
            "¡{name}! ¡Qué nombre tan bonito!|¡Que su familia sea siempre feliz!",
            "Ya somos una familia… ¡Gracias, duendecillo!",
            "¡Dejamos una cuna en su casa para el pequeño! ¡Díganme dónde la pongo!",
        ],
        "f": [
            "Bonjour, vous deux ! Bienvenue à la source de la déesse des récoltes !",
            "La déesse des récoltes est encore pétrifiée… Mais elle a toujours aimé voir des "
            "gens qui s'aiment.",
            "Un bébé est arrivé à la source. Un petit sans famille pour le chérir.",
            "Votre foyer est si plein d'amour…|Voulez-vous élever ce bébé comme le vôtre ?",
            "Un bébé… pour nous ? Oui ! Nous l'aimerons de tout notre cœur !",
            "Super ! Alors ce bébé est le vôtre ! Donnez-lui un nom !",
            "{name} ! Quel joli nom !|Que votre famille soit toujours heureuse !",
            "Nous sommes une famille, maintenant… Merci, petit lutin !",
            "On a laissé un berceau chez vous pour le petit ! Montrez-moi où le mettre !",
        ],
        "g": [
            "Hallo, ihr beiden! Willkommen an der Quelle der Erntegöttin!",
            "Die Erntegöttin ist noch aus Stein … Aber sie hat es immer geliebt, Menschen zu "
            "sehen, die sich lieben.",
            "Ein Baby ist zur Quelle gekommen. Ein Kleines ohne Familie.",
            "Euer Zuhause ist so voller Liebe …|Wollt ihr dieses Baby als euer eigenes großziehen?",
            "Ein Baby … für uns? Ja! Wir werden es von ganzem Herzen lieben!",
            "Juhu! Dann ist dieses Baby eures! Gebt ihm einen Namen!",
            "{name}! Was für ein schöner Name!|Möge eure Familie immer glücklich sein!",
            "Jetzt sind wir eine Familie … Danke, kleiner Wichtel!",
            "Wir haben euch eine Wiege für das Kleine nach Hause gebracht! Zeigt mir, wo sie hin soll!",
        ],
        "i": [
            "Ciao, voi due! Benvenuti alla sorgente della Dea delle messi!",
            "La Dea delle messi è ancora di pietra… Ma ha sempre amato vedere chi si vuole bene.",
            "Alla sorgente è arrivato un bimbo. Un piccolo senza una famiglia che lo abbracci.",
            "La vostra casa è così piena d'amore…|Vorreste crescere questo bimbo come vostro figlio?",
            "Un bimbo… per noi? Sì! Lo ameremo con tutto il cuore!",
            "Evviva! Allora questo bimbo è vostro! Dategli un nome!",
            "{name}! Che bel nome!|Che la vostra famiglia sia sempre felice!",
            "Ora siamo una famiglia… Grazie, piccolo folletto!",
            "Abbiamo lasciato una culla a casa vostra per il piccolo! Mostratemi dove metterla!",
        ],
    },
}
# Where two women need other words than two men; the rest of their lines are the men's.
WOMEN_CHANGES: dict[str, dict[str, dict[int, str]]] = {
    "goddess": {
        "s": {WELCOME: "Bienvenidas a mi manantial. No tengan miedo. Soy la Diosa Cosecha."},
        "i": {WELCOME: "Benvenute alla mia sorgente. Non abbiate paura. Sono la Dea delle messi."},
    },
    "sprite": {
        "s": {WELCOME: "¡Hola a las dos! ¡Bienvenidas al manantial de la Diosa Cosecha!"},
        "i": {WELCOME: "Ciao, voi due! Benvenute alla sorgente della Dea delle messi!"},
    },
}


def font_bytes(text: str) -> bytes:
    out = bytearray()
    for letter in text:
        if letter == "?":
            out.append(FONT_QUESTION_MARK)
        elif letter == "…":
            out.append(FONT_ELLIPSIS)
        else:
            out += letter.encode("cp1252")
    return bytes(out)


def letters(row: str) -> int:
    return len(row.replace("{name}", "N" * NAME_LETTERS))


def message_bytes(visitor: str, speaker: str, text: str) -> bytes:
    """A message: its speaker, its pages of rows wrapped to the window, and its end."""
    pages = []
    for page in text.split("|"):
        rows = textwrap.wrap(
            page.replace("{name}", "\x00" * NAME_LETTERS), LONGEST_ROW, break_on_hyphens=False
        )
        rows = [row.replace("\x00" * NAME_LETTERS, "{name}") for row in rows]
        for start in range(0, len(rows), ROWS_PER_PAGE):
            pages.append(rows[start : start + ROWS_PER_PAGE])
    body = NEW_PAGE.join(
        NEW_ROW.join(font_bytes(row).replace(b"{name}", BABYS_NAME) for row in page) for page in pages
    )
    who = SPEAKS + bytes([VISITORS[visitor][1]]) if speaker == VISITOR_LINE else SPOUSE_SPEAKS
    return who + body + END_OF_MESSAGE


def lines_of(visitor: str, language: str, couple: str) -> list[str]:
    lines = list(LINES[visitor][language])
    if couple == "women":
        for number, text in WOMEN_CHANGES[visitor].get(language, {}).items():
            lines[number] = text
    return lines


def messages_of(visitor: str, language: str, couple: str) -> bytes:
    """The messages of the scene one after another, as the mod writes them."""
    return b"".join(
        message_bytes(visitor, speaker, text)
        for speaker, text in zip(SPEAKERS, lines_of(visitor, language, couple), strict=True)
    )


# ---------------------------------------------------------------------------------------------------------
# The header.


def c_bytes(data: bytes, indent: str = "    ") -> str:
    rows = [
        ", ".join(f"0x{byte:02X}" for byte in data[start : start + 16]) for start in range(0, len(data), 16)
    ]
    return ",\n".join(indent + row for row in rows)


def header_text() -> str:
    parts = [
        "/*",
        " * The adoption scene of marry_anyone: written by tools/make_adoption_scene.py, which tells how it",
        " * is made. Do not change it by hand: change the tool and run it again.",
        " *",
        " * La escena de la adopción de marry_anyone: la escribe tools/make_adoption_scene.py.",
        " */",
        "#pragma once",
        "",
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        f"#define ADOPTION_SPOUSE_PLACEHOLDER 0x{SPOUSE_PLACEHOLDER:02X}u",
        f"#define ADOPTION_MESSAGE_COUNT {len(SPEAKERS)}u",
        "",
        "/* Who brings the baby: the Harvest Goddess once she is awake, a Harvest Sprite while "
        "she is stone. */",
        "typedef enum AdoptionVisitor {",
        *(f"    ADOPTION_BY_THE_{visitor.upper()}," for visitor in VISITORS),
        "} AdoptionVisitor;",
        "",
    ]
    scenes = []
    for visitor in VISITORS:
        script, spouse_places = scene_script(visitor)
        parts += [
            f"/* The script with the {visitor}, as a .lsb file, and where it holds the spouse's number. */",
            f"static const uint8_t adoption_script_{visitor}[] = {{",
            c_bytes(script),
            "};",
            f"static const uint16_t adoption_spouse_places_{visitor}[] = {{"
            + ", ".join(f"0x{place:X}" for place in spouse_places)
            + "};",
            "",
        ]
        scenes.append(
            f"    {{adoption_script_{visitor}, sizeof adoption_script_{visitor},"
            f" adoption_spouse_places_{visitor}, HM_MM_ARRAY_COUNT(adoption_spouse_places_{visitor})}},"
        )
    parts += [
        "typedef struct AdoptionScript {",
        "    const uint8_t* bytes;",
        "    uint32_t size;",
        "    const uint16_t* spouse_places;",
        "    uint32_t spouse_place_count;",
        "} AdoptionScript;",
        "",
        "static const AdoptionScript adoption_scripts[] = {",
        *scenes,
        "};",
        "",
        "/*",
        " * The messages of each visitor, language and couple, one after another, each with its "
        "end. A spouse's",
        " * line starts with 1E and the placeholder, which the mod changes to the spouse's number.",
        " */",
        "typedef struct AdoptionLines {",
        "    AdoptionVisitor visitor;",
        "    char language;",
        "    bool two_women;",
        "    const uint8_t* bytes;",
        "    uint32_t size;",
        "} AdoptionLines;",
        "",
    ]
    table = []
    for visitor in VISITORS:
        for language in LINES[visitor]:
            for couple in ("men", "women"):
                name = f"adoption_lines_{visitor}_{language}_{couple}"
                parts += [
                    f"static const uint8_t {name}[] = {{",
                    c_bytes(messages_of(visitor, language, couple)),
                    "};",
                    "",
                ]
                two_women = "true" if couple == "women" else "false"
                who = f"ADOPTION_BY_THE_{visitor.upper()}"
                table.append(f"    {{{who}, '{language}', {two_women}, {name}, sizeof {name}}},")
    parts += ["static const AdoptionLines adoption_lines[] = {", *table, "};", ""]
    return "\n".join(parts)


def main() -> int:
    OUTPUT.write_text(header_text(), encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(REPOSITORY)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
