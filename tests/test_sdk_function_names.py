"""The 68 functions of the Nintendo SDK, outside GX, that were named by reading what their code does.

Las 68 funciones del SDK de Nintendo, fuera de GX, que se nombraron leyendo lo que hace su código.

Which ones / Cuáles
-------------------
A native runtime replaces, library by library, the SDK functions that the rest of the game reaches: the entry
points that tools/find_library_entry_points.py lists. 39 of them were still unnamed (16 of OS, 9 of CARD,
4 of DVD, 4 of VI, 2 of AI, 2 of DSP, 1 of AR and 1 of PAD). Two internal CARD functions join them, because
the CARD entry points rest on them: __CARDSyncCallback, the callback every synchronous CARD call passes to
its asynchronous twin, and __CARDDefaultApiCallback, the one used when no callback is given.

The audio zone came after: between AXInit and the DSP library only four functions had a name, so the tool
saw almost none of its entry points. Measured by hand, it has 28, and 27 had no name: 14 of AX, 7 of AXFX
(the effects: reverbs, chorus and delay), 4 of MIX (the mixer that sits on AX) and the 2 functions at the
start of the DSP library that tell whether a mail is waiting.

Un runtime nativo reemplaza, biblioteca por biblioteca, las funciones del SDK a las que llega el resto del
juego: los puntos de entrada que lista tools/find_library_entry_points.py. 39 seguían sin nombre (16 de OS,
9 de CARD, 4 de DVD, 4 de VI, 2 de AI, 2 de DSP, 1 de AR y 1 de PAD). Se suman dos funciones internas de
CARD en las que se apoyan sus puntos de entrada: __CARDSyncCallback, la que cada llamada síncrona de CARD
le pasa a su gemela asíncrona, y __CARDDefaultApiCallback, la que se usa cuando no se da ninguna.

Después vino la zona del audio: entre AXInit y la biblioteca DSP solo cuatro funciones tenían nombre, así
que la herramienta casi no veía sus puntos de entrada. Medida a mano tiene 28, y 27 no tenían nombre: 14 de
AX, 7 de AXFX (los efectos: reverberaciones, chorus y delay), 4 de MIX (el mezclador que va sobre AX) y las 2
funciones del inicio de la biblioteca DSP que dicen si hay un correo esperando.

Why the signatures could not name them / Por qué las firmas no pudieron
-----------------------------------------------------------------------
Most are a few instructions long: a getter or a setter of one variable, or an empty function. Their
signature is shared with other functions, so the signature tool names none of them. Four are static
functions that the SDK calls OnReset in four different files; the database gives them all the same name, and
a name must identify one function.

La mayoría tiene pocas instrucciones: un getter o un setter de una variable, o una función vacía. Su firma
la comparten otras funciones, así que la herramienta de firmas no nombra ninguna. Cuatro son funciones
static que el SDK llama OnReset en cuatro archivos distintos; la base les da a todas el mismo nombre, y un
nombre tiene que identificar una sola función.

How each one was identified / Cómo se identificó cada una
---------------------------------------------------------
With facts of its code (tools/code_facts.py) that only one function of its library has: a hardware
register (0xCC006C04 is the AI stream volume, 0xCC00500A the DSP control register, 0xCC004010 the memory
interface, 0xCC00206E the VI status of the digital video output); a word of the OS globals at the start of
memory (0x800000CC the TV mode, 0x800000E4 the current thread, 0x80000000 the disc ID); a variable of the
small-data area (an offset from r13); a message it prints; the functions it calls; or its place in its
source file, for the empty functions and for two functions with the same code. The code and the variables
match the SDK sources that the Twilight Princess decompilation reconstructed (zeldaret/tp,
libs/dolphin/src). 20 of the names (10 of OS and all those of DVD, VI and DSP) are also in the symbol map
of Mario Kart Wii, whose SDK shares those files (the map has no CARD functions).

Con hechos de su código (tools/code_facts.py) que solo tiene una función de su biblioteca: un registro de
hardware, una palabra de las variables globales de OS al inicio de la memoria, una variable del área de
datos pequeños (un desplazamiento de r13), un mensaje que imprime, las funciones que llama, o su lugar en su
archivo fuente, para las funciones vacías y para dos funciones con el mismo código. El código y las
variables coinciden con las fuentes del SDK que reconstruyó la descompilación de Twilight Princess
(zeldaret/tp, libs/dolphin/src). 20 de los nombres (10 de OS y todos los de DVD, VI y DSP) también están en
el mapa de símbolos de Mario Kart Wii, cuyo SDK comparte esos archivos (el mapa no tiene funciones de CARD).

Static functions with the same name / Funciones static con el mismo nombre
--------------------------------------------------------------------------
symbols.txt needs one name per function (the generated code uses the names as identifiers), so a static
function whose name the SDK repeats in several files gets the file's name after its own: OnReset_OSAlarm is
the OnReset of OSAlarm.c.

symbols.txt necesita un nombre por función (el código generado usa los nombres como identificadores), así
que una función static cuyo nombre el SDK repite en varios archivos lleva detrás el nombre del archivo:
OnReset_OSAlarm es la OnReset de OSAlarm.c.

What the tests check / Qué comprueban las pruebas
-------------------------------------------------
Against the real main.dol: every function has the name, the size and the facts of its row; no other
function of its library has them (a row that fitted two functions would identify neither); and the data
behind the names: each OnReset is the function of the reset record that its library registers, the empty
functions are the first values of the variables that hold them, what a function registers is read by
other functions of its library, the variables start with the values that the SDK sources give them, and the
retrace handler counts the retraces that VIGetRetraceCount returns.
"""

import os
import sys
from dataclasses import dataclass
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from code_facts import CodeFacts, Evidence, read_code_facts  # noqa: E402
from find_library_entry_points import functions_in, stretches_of_library  # noqa: E402

from dol import Dol  # noqa: E402
from function_finder import INSTRUCTION_SIZE, find_small_data_bases  # noqa: E402
from ppc import INTEGER_LOAD_OPCODES, Instruction  # noqa: E402
from project import SYMBOLS_PATH  # noqa: E402
from sdk_libraries import library_members, library_runs  # noqa: E402
from symbols import Symbol, SymbolTable  # noqa: E402

DOL_PATH = os.environ.get("HM_MM_DOL")
needs_game_files = pytest.mark.skipif(
    not DOL_PATH or not Path(DOL_PATH).is_file(),
    reason="set HM_MM_DOL to your extracted main.dol",
)

SMALL_DATA_REGISTER = 13

# Hardware registers, as the upper half that a lis loads and the offset that the load or store adds.
HARDWARE_REGISTERS_HIGH_HALF = 0xCC00
AI_REGISTERS_OFFSET = 0x6C00  # 0xCC006C00; the stream volume is the second word, at 0xCC006C04
AI_STREAM_VOLUME_OFFSET = 4
DSP_REGISTERS_OFFSET = 0x5000  # 0xCC005000
DSP_CONTROL_OFFSET = 0x500A  # 0xCC00500A, read with the upper half alone
DSP_CONTROL_OFFSET_FROM_ITS_BLOCK = 0xA
MEMORY_INTERFACE_INTERRUPT_MASK_OFFSET = 0x4010  # 0xCC004010
VI_DTV_STATUS_OFFSET = 0x206E  # 0xCC00206E
# The OS globals at the start of memory (upper half 0x8000).
OS_GLOBALS_HIGH_HALF = 0x8000
TV_MODE_OFFSET = 0xCC
CURRENT_THREAD_OFFSET = 0xE4

# Variables of the small-data area, as offsets from r13 (0x80329080), with their names in the SDK sources.
ALARM_QUEUE_HEAD = -0x6FD0  # OSAlarm.c AlarmQueue.head
CURRENT_HEAP = -0x7698  # OSAlloc.c __OSCurrHeap
ARENA_HIGH = -0x6FB8  # OSArena.c __OSArenaHi
ARENA_LOW = -0x7690  # OSArena.c __OSArenaLo
FONT_ENCODE = -0x7678  # OSFont.c FontEncode, 16 bits
SWITCH_THREAD_CALLBACK = -0x7670  # OSThread.c SwitchThreadCallback
RESCHEDULE = -0x6F40  # OSThread.c Reschedule
AUTO_INVALIDATION = -0x7654  # dvd.c autoInvalidation
CHECK_OPTIONAL_COMMAND = -0x7650  # dvd.c checkOptionalCommand
RETRACE_COUNT = -0x6E5C  # vi.c retraceCount
PRE_RETRACE_CALLBACK = -0x6E4C  # vi.c PreCB
POST_RETRACE_CALLBACK = -0x6E48  # vi.c PostCB
CURRENT_TIMING = -0x6E18  # vi.c CurrTiming
DSP_INIT_FLAG = -0x6C58  # dsp.c __DSP_init_flag

# Other data.
VI_HORIZONTAL_AND_VERTICAL_STATE = 0x802FAA98  # vi.c HorVer
VI_ADJUSTED_DISPLAY_POSITION_Y_OFFSET = 0xA  # HorVer.AdjustedDispPosY
CARD_CONTROL_BLOCKS = 0x80315A20  # CARDBios.c __CARDBlock[2]: one control block per memory card slot
CARD_THREAD_QUEUE_OFFSET = 0x8C  # CARDControl.threadQueue
CARD_RESULT_BUSY = 0xFFFF  # -1, as the 16 bits of a cmpwi
CARD_DEFAULT_API_CALLBACK = 0x80188300
CARD_SYNC_CALLBACK = 0x80188304
ALL_FOUR_CONTROLLERS = 0xF000  # PAD_CHAN0_BIT | ... | PAD_CHAN3_BIT, the upper half of 0xF0000000
NO_CONTROLLER_RESETTING = 0x20  # PADSync: ResettingChan == 32
SRAM_SIZE = 0x14  # sizeof(OSSram)
INTERRUPTS = {"OSDisableInterrupts", "OSRestoreInterrupts"}

# AX. A voice is an AXVPB; the parameter block that the DSP reads (AXPB) starts at +0x138 of it (ax.h).
VOICE_PRIORITY_OFFSET = 0xC
VOICE_CALLBACK_OFFSET = 0x10
VOICE_USER_CONTEXT_OFFSET = 0x14
VOICE_SYNC_OFFSET = 0x1C  # flags saying which parts of the block to copy to the DSP on the next frame
VOICE_DEPOP_OFFSET = 0x20
VOICE_PARAMETER_BLOCK_OFFSET = 0x138
SOURCE_SELECT_OFFSET_IN_BLOCK = 0x8  # pb.srcSelect
COEFFICIENT_SELECT_OFFSET_IN_BLOCK = 0xA  # pb.coefSelect
VOICE_MIXER_CONTROL_OFFSET = 0x144  # pb.mixerCtrl
VOICE_STATE_OFFSET = 0x146  # pb.state
VOICE_TYPE_OFFSET = 0x148  # pb.type
VOICE_MIX_OFFSET = 0x14A  # pb.mix
VOICE_ADDRESS_OFFSET = 0x1A6  # pb.addr
VOICE_ADPCM_OFFSET = 0x1B6  # pb.adpcm
VOICE_SAMPLE_RATE_CONVERTER_OFFSET = 0x1DE  # pb.src; its first two halves are the ratio
VOICE_SAMPLE_RATE_RATIO_LOW_OFFSET = 0x1E0
VOICE_ADPCM_LOOP_OFFSET = 0x1EC  # pb.adpcmLoop
SYNC_COPY_STATE = 0x4  # AX_SYNC_FLAG_COPYSTATE
SYNC_COPY_TYPE = 0x8  # AX_SYNC_FLAG_COPYTYPE
SYNC_COPY_MIX = 0x10  # AX_SYNC_FLAG_COPYAXPBMIX
SYNC_COPY_ADDRESS = 0x1000  # AX_SYNC_FLAG_COPYADDR
VOICE_STACK_HEADS = 0x802FAB40  # AXAlloc.c __AXStackHead[32]: one stack of voices per priority
AUX_A_CALLBACK = -0x6D38  # AXAux.c __AXCallbackAuxA
AUX_B_CALLBACK = -0x6D34  # __AXCallbackAuxB
AUX_A_CONTEXT = -0x6D30  # __AXContextAuxA
AUX_B_CONTEXT = -0x6D2C  # __AXContextAuxB
COMMAND_LIST_MODE = -0x6CF0  # AXCL.c __AXClMode
USER_FRAME_CALLBACK = -0x6CD0  # AXOut.c __AXUserFrameCallback
# AXFX: the effects take their memory through two hooks (axfx.c).
AXFX_ALLOC_HOOK = -0x75D8  # __AXFXAlloc
AXFX_FREE_HOOK = -0x75D4  # __AXFXFree
REVERB_HI_TEMPORARILY_DISABLED_OFFSET = 0x1C4  # AXFX_REVERBHI.tempDisableFX
REVERB_STD_TEMPORARILY_DISABLED_OFFSET = 0x13C  # AXFX_REVERBSTD.tempDisableFX
DELAY_BUFFER_OFFSETS = {0x30, 0x34, 0x38}  # AXFX_DELAY.left, .right and .sur
# MIX.
MIX_CHANNELS = 0x80314220  # mix.c __MIXChannel[64], one per voice
MIX_DVD_STREAM_ATTENUATION_NOW = -0x6C68  # __MIXDvdStreamAttenCurrent
MIX_DVD_STREAM_ATTENUATION_WANTED = -0x6C64  # __MIXDvdStreamAttenUser
MIX_SOUND_MODE = -0x6C60  # __MIXSoundMode
MIX_RESET_MODE_HIGH_HALF = 0x5000  # a reset channel's mode is 0x50000000
MIX_RESET_AUX_LEVEL = 0xFC40  # -0x3C0: the aux sends of a reset channel start at -96 dB
VOICE_INDEX_OFFSET = 0x18  # AXVPB.index: the voice's channel
# DSP: the mailboxes are the first registers of 0xCC005000; bit 15 of the high half says "a mail is there".
DSP_MAIL_TO_DSP_OFFSET = 0x5000
DSP_MAIL_FROM_DSP_OFFSET = 0x5004

EVIDENCE = [
    # ---- OS ----
    # alarm->handler = 0; alarm->tag = 0. OSInitThreadQueue has the same code (its two stores are in the other
    # order): the place in OSAlarm.c, between OSInitAlarm and InsertAlarm, tells them apart.
    Evidence(
        0x8016F8C4,
        "OSCreateAlarm",
        0x10,
        stored_offsets={0, 4},
        comes_after="OSInitAlarm",
        comes_before="InsertAlarm",
    ),
    # When the reset is final, cancels every alarm of the queue except those __DVDTestAlarm keeps.
    Evidence(
        0x8016FF28,
        "OnReset_OSAlarm",
        0xA0,
        loaded_offsets={ALARM_QUEUE_HEAD},
        callees={"__DVDTestAlarm", "OSCancelAlarm"},
    ),
    # Swaps the current heap and returns the one before.
    Evidence(
        0x801701EC,
        "OSSetCurrentHeap",
        0x10,
        loaded_offsets={CURRENT_HEAP},
        stored_offsets={CURRENT_HEAP},
    ),
    Evidence(
        0x801702D8,
        "OSCheckHeap",
        0x360,
        strings={"OSCheckHeap: Failed HeapArray in %d"},
        callees={"OSReport"},
    ),
    # The four bounds of the arena: two getters and two setters of two variables.
    Evidence(0x80170638, "OSGetArenaHi", 0x8, loaded_offsets={ARENA_HIGH}),
    Evidence(0x80170640, "OSGetArenaLo", 0x8, loaded_offsets={ARENA_LOW}),
    Evidence(0x80170648, "OSSetArenaHi", 0x8, stored_offsets={ARENA_HIGH}),
    Evidence(0x80170650, "OSSetArenaLo", 0x8, stored_offsets={ARENA_LOW}),
    # Shift-JIS when the TV mode is NTSC and the digital video output says so, else ANSI; kept in a 16-bit
    # variable that starts at 0xFFFF ("not known yet").
    Evidence(
        0x80172684,
        "OSGetFontEncode",
        0x64,
        constants={OS_GLOBALS_HIGH_HALF, HARDWARE_REGISTERS_HIGH_HALF},
        loaded_offsets={FONT_ENCODE, TV_MODE_OFFSET, VI_DTV_STATUS_OFFSET},
        stored_offsets={FONT_ENCODE},
    ),
    # When the reset is final, masks the memory interface interrupts: 0xFF to 0xCC004010.
    Evidence(
        0x801730A8,
        "OnReset_OSMemory",
        0x3C,
        constants={HARDWARE_REGISTERS_HIGH_HALF, 0xFF},
        stored_offsets={MEMORY_INTERFACE_INTERRUPT_MASK_OFFSET},
        callees={"__OSMaskInterrupts"},
    ),
    # UnlockSram(commit, 0) and UnlockSram(commit, sizeof(OSSram)).
    Evidence(0x8017432C, "__OSUnlockSram", 0x24, constants={0}, callees={"UnlockSram"}),
    Evidence(0x80174350, "__OSUnlockSramEx", 0x24, constants={SRAM_SIZE}, callees={"UnlockSram"}),
    # An empty function at the start of OSThread.c (see the test of the default callbacks).
    Evidence(
        0x801747F0,
        "DefaultSwitchThreadCallback",
        0x4,
        comes_after="__OSInitSystemCall",
        comes_before="__OSThreadInit",
    ),
    # queue->head = queue->tail = NULL, in OSThread.c right before OSGetCurrentThread.
    Evidence(
        0x8017494C,
        "OSInitThreadQueue",
        0x10,
        stored_offsets={0, 4},
        comes_after="__OSThreadInit",
        comes_before="OSGetCurrentThread",
    ),
    Evidence(
        0x8017495C,
        "OSGetCurrentThread",
        0xC,
        constants={OS_GLOBALS_HIGH_HALF},
        loaded_offsets={CURRENT_THREAD_OFFSET},
    ),
    # Reschedule-- with the interrupts off (0xFFFF is the -1). OSDisableScheduler, right before it, adds 1.
    Evidence(
        0x801749A8,
        "OSEnableScheduler",
        0x40,
        constants={0xFFFF},
        loaded_offsets={RESCHEDULE},
        stored_offsets={RESCHEDULE},
        callees=INTERRUPTS,
    ),
    # ---- DVD ----
    Evidence(
        0x80176B70,
        "DVDOpen",
        0xC8,
        strings={"Warning: DVDOpen(): file '%s' was not found under %s."},
        callees={"DVDConvertPathToEntrynum", "OSReport"},
    ),
    # An empty function at the start of dvd.c (see the test of the default callbacks).
    Evidence(0x80176F70, "defaultOptionalCommandChecker", 0x4, comes_before="DVDInit"),
    Evidence(
        0x801790F8,
        "DVDSetAutoInvalidation",
        0x10,
        loaded_offsets={AUTO_INVALIDATION},
        stored_offsets={AUTO_INVALIDATION},
    ),
    # Returns 0x80000000, where the disc ID is copied at boot.
    Evidence(0x801794A4, "DVDGetCurrentDiskID", 0x8, constants={OS_GLOBALS_HIGH_HALF}),
    # ---- VI ----
    # The game has no VISetPreRetraceCallback; this one sets the callback the handler calls last.
    Evidence(
        0x8017A064,
        "VISetPostRetraceCallback",
        0x44,
        loaded_offsets={POST_RETRACE_CALLBACK},
        stored_offsets={POST_RETRACE_CALLBACK},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x8017ACCC,
        "VIConfigure",
        0x808,
        strings={"VIConfigure(): Tried to change mode from (%d) to (%d), which is forbidden"},
        callees={"setFbbRegs", "setVerticalRegs"},
    ),
    Evidence(0x8017B6EC, "VIGetRetraceCount", 0x8, loaded_offsets={RETRACE_COUNT}),
    # getCurrentFieldEvenOdd() ^ 1, with the interrupts off, then ^ (HorVer.AdjustedDispPosY & 1).
    Evidence(
        0x8017B798,
        "VIGetNextField",
        0x9C,
        addresses={VI_HORIZONTAL_AND_VERTICAL_STATE},
        loaded_offsets={CURRENT_TIMING, VI_ADJUSTED_DISPLAY_POSITION_Y_OFFSET},
        callees=INTERRUPTS,
        comes_after="getCurrentFieldEvenOdd",
        comes_before="VIGetCurrentLine",
    ),
    # ---- PAD ----
    # Drops the sampling callback; before the final reset, waits for the controllers, then recalibrates
    # all four.
    Evidence(
        0x8017D550,
        "OnReset_Pad",
        0xBC,
        constants={ALL_FOUR_CONTROLLERS, NO_CONTROLLER_RESETTING},
        callees={"PADSetSamplingCallback", "SIBusy", "PADRecalibrate"},
    ),
    # ---- AI ----
    # The left volume is the lowest byte of 0xCC006C04, the right volume the byte above it.
    Evidence(
        0x8017DAE0,
        "AISetStreamVolLeft",
        0x1C,
        constants={HARDWARE_REGISTERS_HIGH_HALF, AI_REGISTERS_OFFSET},
        loaded_offsets={AI_STREAM_VOLUME_OFFSET},
        stored_offsets={AI_STREAM_VOLUME_OFFSET},
        inserted_fields={(0, 24, 31)},
    ),
    Evidence(
        0x8017DB0C,
        "AISetStreamVolRight",
        0x1C,
        constants={HARDWARE_REGISTERS_HIGH_HALF, AI_REGISTERS_OFFSET},
        loaded_offsets={AI_STREAM_VOLUME_OFFSET},
        stored_offsets={AI_STREAM_VOLUME_OFFSET},
        inserted_fields={(8, 16, 23)},
    ),
    # ---- AR ----
    # Reads the DSP control register (the ARAM DMA interrupt is its bit 0x20) and writes nothing.
    Evidence(
        0x8017E33C,
        "__ARGetInterruptStatus",
        0x10,
        constants={HARDWARE_REGISTERS_HIGH_HALF},
        loaded_offsets={DSP_CONTROL_OFFSET},
    ),
    # ---- DSP ----
    Evidence(0x801878EC, "DSPCheckInit", 0x8, loaded_offsets={DSP_INIT_FLAG}),
    # Makes a task the next to run and, if another is running, interrupts the DSP: it clears three bits of the
    # control register (0xFF57 keeps the others) and sets bit 1.
    Evidence(
        0x80187964,
        "DSPAssertTask",
        0xC8,
        constants={HARDWARE_REGISTERS_HIGH_HALF, DSP_REGISTERS_OFFSET, 0xFF57},
        stored_offsets={DSP_CONTROL_OFFSET_FROM_ITS_BLOCK},
        callees=INTERRUPTS,
    ),
    # ---- CARD ----
    # An empty function at the start of CARDBios.c (see the test of the default callbacks).
    Evidence(0x80188300, "__CARDDefaultApiCallback", 0x4, comes_before="__CARDSyncCallback"),
    # The pair that makes a synchronous call out of an asynchronous one: __CARDSync sleeps on the thread queue
    # of the slot's control block until the result is no longer "busy", and the callback wakes it.
    Evidence(
        0x80188304,
        "__CARDSyncCallback",
        0x34,
        addresses={CARD_CONTROL_BLOCKS},
        constants={CARD_THREAD_QUEUE_OFFSET},
        callees={"OSWakeupThread"},
    ),
    Evidence(
        0x801896F0,
        "__CARDSync",
        0x98,
        addresses={CARD_CONTROL_BLOCKS},
        constants={CARD_THREAD_QUEUE_OFFSET, CARD_RESULT_BUSY},
        callees={"OSSleepThread"} | INTERRUPTS,
    ),
    # Before the final reset, unmounts both slots; a busy card delays the reset.
    Evidence(
        0x80189788,
        "OnReset_CARDBios",
        0x50,
        constants={CARD_RESULT_BUSY},
        callees={"CARDUnmount"},
    ),
    # Each synchronous call passes __CARDSyncCallback to its asynchronous twin and waits with __CARDSync.
    # CARDCheck keeps the byte count that CARDCheckEx would return in a local of its own (at 0xC on the
    # stack).
    Evidence(
        0x8018C260,
        "CARDCheck",
        0x54,
        addresses={CARD_SYNC_CALLBACK},
        constants={0xC},
        callees={"CARDCheckExAsync", "__CARDSync"},
    ),
    Evidence(
        0x8018CC28,
        "CARDMount",
        0x48,
        addresses={CARD_SYNC_CALLBACK},
        callees={"CARDMountAsync", "__CARDSync"},
    ),
    Evidence(
        0x8018D554,
        "CARDFormat",
        0x54,
        addresses={CARD_SYNC_CALLBACK},
        callees={"__CARDFormatRegionAsync", "__CARDGetFontEncode", "__CARDSync"},
    ),
    Evidence(
        0x8018DEE4,
        "CARDCreate",
        0x48,
        addresses={CARD_SYNC_CALLBACK},
        callees={"CARDCreateAsync", "__CARDSync"},
    ),
    Evidence(
        0x8018E358,
        "CARDRead",
        0x48,
        addresses={CARD_SYNC_CALLBACK},
        callees={"CARDReadAsync", "__CARDSync"},
    ),
    Evidence(
        0x8018E6D4,
        "CARDWrite",
        0x48,
        addresses={CARD_SYNC_CALLBACK},
        callees={"CARDWriteAsync", "__CARDSync"},
    ),
    Evidence(
        0x8018E8D0,
        "CARDDelete",
        0x48,
        addresses={CARD_SYNC_CALLBACK},
        callees={"CARDDeleteAsync", "__CARDSync"},
    ),
    # ---- AX ----
    # Takes a voice from the stacks of the priorities, with the interrupts off, and keeps the priority, the
    # callback and the user's context in it.
    Evidence(
        0x8018016C,
        "AXAcquireVoice",
        0x174,
        addresses={VOICE_STACK_HEADS},
        stored_offsets={VOICE_PRIORITY_OFFSET, VOICE_CALLBACK_OFFSET, VOICE_USER_CONTEXT_OFFSET},
        callees=INTERRUPTS,
    ),
    # The two aux sends: each keeps a callback and its context (see the test of the registered callbacks).
    Evidence(0x80180758, "AXRegisterAuxACallback", 0xC, stored_offsets={AUX_A_CALLBACK, AUX_A_CONTEXT}),
    Evidence(0x80180764, "AXRegisterAuxBCallback", 0xC, stored_offsets={AUX_B_CALLBACK, AUX_B_CONTEXT}),
    Evidence(
        0x80180E88,
        "AXSetMode",
        0x14,
        loaded_offsets={COMMAND_LIST_MODE},
        stored_offsets={COMMAND_LIST_MODE},
    ),
    # Swaps the function called on every audio frame, with the interrupts off, and returns the one before.
    Evidence(
        0x8018160C,
        "AXRegisterCallback",
        0x44,
        loaded_offsets={USER_FRAME_CALLBACK},
        stored_offsets={USER_FRAME_CALLBACK},
        callees=INTERRUPTS,
    ),
    # The voice setters: each writes its part of the parameter block with the interrupts off and marks it in
    # the sync flags, so that the next frame copies it to the DSP. The part tells them apart.
    Evidence(
        0x80182734,
        "AXSetVoiceSrcType",
        0xC4,
        constants={VOICE_PARAMETER_BLOCK_OFFSET},
        loaded_offsets={VOICE_SYNC_OFFSET},
        stored_offsets={SOURCE_SELECT_OFFSET_IN_BLOCK, COEFFICIENT_SELECT_OFFSET_IN_BLOCK, VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    # Stopping a voice also asks for a depop.
    Evidence(
        0x801827F8,
        "AXSetVoiceState",
        0x5C,
        constants={SYNC_COPY_STATE},
        stored_offsets={VOICE_STATE_OFFSET, VOICE_DEPOP_OFFSET, VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x80182854,
        "AXSetVoiceType",
        0x4C,
        constants={SYNC_COPY_TYPE},
        stored_offsets={VOICE_TYPE_OFFSET, VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x801828A0,
        "AXSetVoiceMix",
        0x254,
        constants={VOICE_MIX_OFFSET, SYNC_COPY_MIX},
        stored_offsets={VOICE_MIXER_CONTROL_OFFSET, VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x80182AF4,
        "AXSetVoiceAddr",
        0x110,
        constants={VOICE_ADDRESS_OFFSET, SYNC_COPY_ADDRESS},
        stored_offsets={VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x80182C04,
        "AXSetVoiceAdpcm",
        0xA4,
        constants={VOICE_ADPCM_OFFSET},
        stored_offsets={VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x80182CA8,
        "AXSetVoiceSrc",
        0x98,
        constants={VOICE_SAMPLE_RATE_CONVERTER_OFFSET},
        stored_offsets={VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    # Turns a float ratio into the two halves of the converter's ratio.
    Evidence(
        0x80182D40,
        "AXSetVoiceSrcRatio",
        0x98,
        stored_offsets={
            VOICE_SAMPLE_RATE_CONVERTER_OFFSET,
            VOICE_SAMPLE_RATE_RATIO_LOW_OFFSET,
            VOICE_SYNC_OFFSET,
        },
        callees=INTERRUPTS,
    ),
    Evidence(
        0x80182DD8,
        "AXSetVoiceAdpcmLoop",
        0x6C,
        constants={VOICE_ADPCM_LOOP_OFFSET},
        stored_offsets={VOICE_SYNC_OFFSET},
        callees=INTERRUPTS,
    ),
    # ---- AXFX ----
    # Each init clears its "temporarily disabled" flag and builds the effect with the interrupts off.
    Evidence(
        0x80183BD4,
        "AXFXReverbHiInit",
        0x70,
        stored_offsets={REVERB_HI_TEMPORARILY_DISABLED_OFFSET},
        callees=INTERRUPTS,
    ),
    Evidence(
        0x8018469C,
        "AXFXReverbStdInit",
        0x6C,
        stored_offsets={REVERB_STD_TEMPORARILY_DISABLED_OFFSET},
        callees=INTERRUPTS,
    ),
    # Takes its delay lines from the allocation hook; it follows the two sample-rate converters of chorus.c.
    Evidence(
        0x80184ABC,
        "AXFXChorusInit",
        0x248,
        loaded_offsets={AXFX_ALLOC_HOOK},
        callees=INTERRUPTS,
        comes_after="do_src2",
    ),
    # Forgets the three delay buffers, then applies the settings.
    Evidence(
        0x801855FC,
        "AXFXDelayInit",
        0x48,
        stored_offsets=DELAY_BUFFER_OFFSETS,
        callees=INTERRUPTS,
    ),
    # The default hooks: memory from and to the current heap (see the test of the default callbacks).
    Evidence(
        0x801856E4,
        "__AXFXAllocFunction",
        0x28,
        loaded_offsets={CURRENT_HEAP},
        callees={"OSAllocFromHeap"},
    ),
    Evidence(
        0x8018570C,
        "__AXFXFreeFunction",
        0x28,
        loaded_offsets={CURRENT_HEAP},
        callees={"OSFreeToHeap"},
    ),
    Evidence(0x80185734, "AXFXSetHooks", 0xC, stored_offsets={AXFX_ALLOC_HOOK, AXFX_FREE_HOOK}),
    # ---- MIX ----
    # Resets the 64 channels (mode 0x50000000, aux sends at -96 dB), the DVD stream fader and the sound mode.
    Evidence(
        0x801857FC,
        "MIXInit",
        0xC8,
        addresses={MIX_CHANNELS},
        constants={MIX_RESET_MODE_HIGH_HALF, MIX_RESET_AUX_LEVEL},
        stored_offsets={MIX_DVD_STREAM_ATTENUATION_NOW, MIX_DVD_STREAM_ATTENUATION_WANTED, MIX_SOUND_MODE},
    ),
    Evidence(
        0x801858C4,
        "MIXInitChannel",
        0xE7C,
        addresses={MIX_CHANNELS},
        loaded_offsets={MIX_SOUND_MODE},
        callees=INTERRUPTS,
    ),
    # __MIXChannel[voice->index].axvpb = NULL.
    Evidence(
        0x80186740,
        "MIXReleaseChannel",
        0x20,
        addresses={MIX_CHANNELS},
        loaded_offsets={VOICE_INDEX_OFFSET},
        stored_offsets={0},
    ),
    # Brings the DVD stream fader one step closer to what was asked, through the AI stream volume.
    Evidence(
        0x80186760,
        "MIXUpdateSettings",
        0x107C,
        loaded_offsets={MIX_DVD_STREAM_ATTENUATION_WANTED},
        stored_offsets={MIX_DVD_STREAM_ATTENUATION_NOW},
        callees={"AISetStreamVolLeft", "AISetStreamVolRight"},
    ),
    # ---- DSP ----
    Evidence(
        0x801877DC,
        "DSPCheckMailToDSP",
        0x10,
        constants={HARDWARE_REGISTERS_HIGH_HALF},
        loaded_offsets={DSP_MAIL_TO_DSP_OFFSET},
    ),
    Evidence(
        0x801877EC,
        "DSPCheckMailFromDSP",
        0x10,
        constants={HARDWARE_REGISTERS_HIGH_HALF},
        loaded_offsets={DSP_MAIL_FROM_DSP_OFFSET},
        comes_before="DSPReadMailFromDSP",
    ),
]


@dataclass(frozen=True)
class ResetRecord:
    """An OSResetFunctionInfo: the function OSResetSystem calls, its priority, and who registers it."""

    address: int
    function: str
    priority: int
    registered_by: str


RESET_RECORDS = [
    ResetRecord(0x802A3AB0, "OnReset_OSAlarm", 0xFFFFFFFF, "OSInitAlarm"),
    ResetRecord(0x802A5150, "OnReset_OSMemory", 0x7F, "__OSInitMemoryProtection"),
    ResetRecord(0x802A5958, "OnReset_Pad", 0x7F, "PADInit"),
    ResetRecord(0x802AAA98, "OnReset_CARDBios", 0x7F, "CARDInit"),
]


@dataclass(frozen=True)
class DefaultCallback:
    """An empty function that a variable of the small-data area holds until the game sets its own."""

    variable_offset: int
    function: str
    library: str


DEFAULT_CALLBACKS = [
    DefaultCallback(SWITCH_THREAD_CALLBACK, "DefaultSwitchThreadCallback", "OS"),
    DefaultCallback(CHECK_OPTIONAL_COMMAND, "defaultOptionalCommandChecker", "DVD"),
    DefaultCallback(AXFX_ALLOC_HOOK, "__AXFXAllocFunction", "AXFX"),
    DefaultCallback(AXFX_FREE_HOOK, "__AXFXFreeFunction", "AXFX"),
]


@dataclass(frozen=True)
class RegisteredValue:
    """A function that keeps what the game gives it in variables that other functions of its library read."""

    function: str
    variable_offsets: frozenset[int]


REGISTERED_VALUES = [
    RegisteredValue("AXRegisterAuxACallback", frozenset({AUX_A_CALLBACK, AUX_A_CONTEXT})),
    RegisteredValue("AXRegisterAuxBCallback", frozenset({AUX_B_CALLBACK, AUX_B_CONTEXT})),
    RegisteredValue("AXRegisterCallback", frozenset({USER_FRAME_CALLBACK})),
    RegisteredValue("AXSetMode", frozenset({COMMAND_LIST_MODE})),
    RegisteredValue("AXFXSetHooks", frozenset({AXFX_ALLOC_HOOK, AXFX_FREE_HOOK})),
    RegisteredValue("VISetPostRetraceCallback", frozenset({POST_RETRACE_CALLBACK})),
]


@dataclass(frozen=True)
class FirstValue:
    """The value a variable of the small-data area has when the game starts, as the SDK sources set it.

    None means the variable is in .sbss: it has no stored value and starts at zero."""

    name: str
    variable_offset: int
    size: int  # in bytes
    first_value: int | None


FIRST_VALUES = [
    FirstValue("__OSArenaHi", ARENA_HIGH, 4, None),
    FirstValue("__OSArenaLo", ARENA_LOW, 4, 0xFFFFFFFF),  # (void*)-1: "OSInit has not run yet"
    FirstValue("__OSCurrHeap", CURRENT_HEAP, 4, 0xFFFFFFFF),  # -1: no heap
    FirstValue("FontEncode", FONT_ENCODE, 2, 0xFFFF),  # not known yet
    FirstValue("autoInvalidation", AUTO_INVALIDATION, 4, 1),  # TRUE
]


def test_the_table_has_68_functions_with_different_names_and_addresses():
    assert len(EVIDENCE) == 68
    assert len({evidence.name for evidence in EVIDENCE}) == 68
    assert len({evidence.address for evidence in EVIDENCE}) == 68


def test_a_repeated_static_name_carries_the_name_of_its_file():
    on_reset_names = sorted(evidence.name for evidence in EVIDENCE if evidence.name.startswith("OnReset"))
    assert on_reset_names == ["OnReset_CARDBios", "OnReset_OSAlarm", "OnReset_OSMemory", "OnReset_Pad"]
    assert sorted(record.function for record in RESET_RECORDS) == on_reset_names


# ---------------------------------------------------------------- the real main.dol


@pytest.fixture(scope="module")
def symbols() -> SymbolTable:
    return SymbolTable.read(SYMBOLS_PATH)


@pytest.fixture(scope="module")
def dol() -> Dol:
    return Dol(Path(DOL_PATH))


@pytest.fixture(scope="module")
def small_data_bases(dol) -> dict[int, int]:
    return find_small_data_bases(dol)


def library_holding(symbols: SymbolTable, address: int) -> str:
    """The library whose stretch of named functions holds `address`."""
    for run in library_runs(library_members(symbols)):
        if run.contains(address):
            return run.library
    raise AssertionError(f"0x{address:08X} is in no library's stretch")


@pytest.fixture(scope="module")
def facts_by_library(dol, symbols, small_data_bases) -> dict[str, dict[int, CodeFacts]]:
    """The facts of every function of each library that holds a row of the table, by address."""
    libraries = {library_holding(symbols, evidence.address) for evidence in EVIDENCE}
    return {
        library: {
            function.address: read_code_facts(dol, function, symbols, small_data_bases)
            for function in functions_in(symbols, stretches_of_library(symbols, library))
        }
        for library in libraries
    }


def facts_of(name: str, dol: Dol, symbols: SymbolTable, small_data_bases: dict[int, int]) -> CodeFacts:
    function = symbols.by_name(name)
    assert function is not None, f"{name} is not in symbols.txt"
    return read_code_facts(dol, function, symbols, small_data_bases)


@needs_game_files
@pytest.mark.parametrize("evidence", EVIDENCE, ids=lambda evidence: evidence.name)
def test_the_function_has_the_name_the_size_and_the_facts_of_the_table(
    evidence, dol, symbols, small_data_bases
):
    symbol = symbols.by_name(evidence.name)
    assert symbol is not None, f"{evidence.name} is not in symbols.txt"
    assert symbol.address == evidence.address
    assert symbol.size == evidence.size
    missing = evidence.missing_facts(read_code_facts(dol, symbol, symbols, small_data_bases))
    assert not missing, f"not found in the code: {missing}"


@needs_game_files
@pytest.mark.parametrize("evidence", EVIDENCE, ids=lambda evidence: evidence.name)
def test_no_other_function_of_its_library_has_the_same_facts(evidence, symbols, facts_by_library):
    library = library_holding(symbols, evidence.address)
    fitting = [address for address, facts in facts_by_library[library].items() if evidence.is_met_by(facts)]
    assert fitting == [evidence.address], f"in {library}: {[f'0x{address:08X}' for address in fitting]}"


# The data behind the names.


@needs_game_files
@pytest.mark.parametrize("record", RESET_RECORDS, ids=lambda record: record.function)
def test_each_on_reset_is_the_function_of_the_reset_record_its_library_registers(
    record, dol, symbols, small_data_bases
):
    function_word, priority_word = dol.u32(record.address), dol.u32(record.address + 4)
    assert function_word == symbols.by_name(record.function).address
    assert priority_word == record.priority
    registering = facts_of(record.registered_by, dol, symbols, small_data_bases)
    assert record.address in registering.addresses
    assert "OSRegisterResetFunction" in registering.callees


@needs_game_files
@pytest.mark.parametrize("default", DEFAULT_CALLBACKS, ids=lambda default: default.function)
def test_each_empty_function_is_the_first_value_of_the_variable_that_holds_it(
    default, dol, symbols, small_data_bases, facts_by_library
):
    variable = small_data_bases[SMALL_DATA_REGISTER] + default.variable_offset
    assert dol.u32(variable) == symbols.by_name(default.function).address
    readers = [
        address
        for address, facts in facts_by_library[default.library].items()
        if default.variable_offset in facts.loaded_offsets
    ]
    assert readers, f"no function of {default.library} reads the variable at 0x{variable:08X}"


@needs_game_files
def test_the_asynchronous_card_calls_fall_back_on_the_empty_callback(dol, symbols, small_data_bases):
    for name in (
        "CARDMountAsync",
        "CARDCreateAsync",
        "CARDReadAsync",
        "CARDWriteAsync",
        "CARDDeleteAsync",
        "__CARDFormatRegionAsync",
    ):
        assert CARD_DEFAULT_API_CALLBACK in facts_of(name, dol, symbols, small_data_bases).addresses, name


@needs_game_files
@pytest.mark.parametrize("registered", REGISTERED_VALUES, ids=lambda registered: registered.function)
def test_what_a_function_registers_is_read_by_other_functions_of_its_library(
    registered, symbols, facts_by_library
):
    setter = symbols.by_name(registered.function)
    library = library_holding(symbols, setter.address)
    for offset in sorted(registered.variable_offsets):
        readers = [
            address
            for address, facts in facts_by_library[library].items()
            if address != setter.address and offset in facts.loaded_offsets
        ]
        assert readers, f"no other function of {library} reads the variable at {offset:#x}(r13)"


@needs_game_files
@pytest.mark.parametrize("variable", FIRST_VALUES, ids=lambda variable: variable.name)
def test_the_variables_start_with_the_values_of_the_sdk_sources(variable, dol, small_data_bases):
    address = small_data_bases[SMALL_DATA_REGISTER] + variable.variable_offset
    if variable.first_value is None:
        assert dol.section_at(address) is None and dol.is_mapped(address), "expected in .sbss"
    else:
        stored = dol.u32(address) if variable.size == 4 else dol.u16(address)
        assert stored == variable.first_value


def first_load_position(dol: Dol, function: Symbol, offset: int) -> int | None:
    """The index of the first instruction of `function` that loads the small-data variable at `offset`."""
    for index in range(function.size // INSTRUCTION_SIZE):
        address = function.address + index * INSTRUCTION_SIZE
        instruction = Instruction(address, dol.u32(address))
        if (
            instruction.opcode in INTEGER_LOAD_OPCODES
            and instruction.ra == SMALL_DATA_REGISTER
            and instruction.displacement == offset
        ):
            return index
    return None


@needs_game_files
def test_the_retrace_handler_counts_the_retraces_and_calls_the_post_callback_last(
    dol, symbols, small_data_bases
):
    handler_symbol = symbols.by_name("__VIRetraceHandler")
    handler = read_code_facts(dol, handler_symbol, symbols, small_data_bases)
    assert RETRACE_COUNT in handler.loaded_offsets and RETRACE_COUNT in handler.stored_offsets
    assert RETRACE_COUNT in facts_of("VIGetRetraceCount", dol, symbols, small_data_bases).loaded_offsets
    pre_callback = first_load_position(dol, handler_symbol, PRE_RETRACE_CALLBACK)
    post_callback = first_load_position(dol, handler_symbol, POST_RETRACE_CALLBACK)
    assert pre_callback is not None and post_callback is not None
    assert pre_callback < post_callback


@needs_game_files
def test_dsp_check_init_reads_the_flag_that_dsp_init_sets(dol, symbols, small_data_bases):
    assert DSP_INIT_FLAG in facts_of("DSPInit", dol, symbols, small_data_bases).stored_offsets
    assert DSP_INIT_FLAG in facts_of("DSPCheckInit", dol, symbols, small_data_bases).loaded_offsets
