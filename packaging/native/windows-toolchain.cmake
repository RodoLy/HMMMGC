# Cross-compiling for Windows on Linux with llvm-mingw (https://github.com/mstorsjo/llvm-mingw), for
# the native package: its runtime kit and its launcher (docs/NATIVE_PACKAGE.md).
#
# Compilar para Windows desde Linux con llvm-mingw, para el paquete nativo: su kit del runtime y su
# launcher.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=packaging/native/windows-toolchain.cmake \
#         -DLLVM_MINGW=<llvm-mingw for Linux> -DHMMM_WINDOWS_ARCHITECTURE=arm64|x64 ...
#
# LLVM_MINGW and HMMM_WINDOWS_ARCHITECTURE can also come from the environment.
if(NOT LLVM_MINGW)
    set(LLVM_MINGW "$ENV{LLVM_MINGW}")
endif()
if(NOT HMMM_WINDOWS_ARCHITECTURE)
    set(HMMM_WINDOWS_ARCHITECTURE "$ENV{HMMM_WINDOWS_ARCHITECTURE}")
endif()
# CMake reads this file again for its own test builds, which only see these variables if told.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW HMMM_WINDOWS_ARCHITECTURE)

if(HMMM_WINDOWS_ARCHITECTURE STREQUAL "arm64")
    set(hmmm_triple aarch64-w64-mingw32)
    set(CMAKE_SYSTEM_PROCESSOR ARM64)
elseif(HMMM_WINDOWS_ARCHITECTURE STREQUAL "x64")
    set(hmmm_triple x86_64-w64-mingw32)
    set(CMAKE_SYSTEM_PROCESSOR AMD64)
else()
    message(FATAL_ERROR "HMMM_WINDOWS_ARCHITECTURE must be arm64 or x64, not '${HMMM_WINDOWS_ARCHITECTURE}'")
endif()
if(NOT EXISTS "${LLVM_MINGW}/bin/${hmmm_triple}-clang")
    message(FATAL_ERROR "LLVM_MINGW must be an llvm-mingw for Linux (no ${LLVM_MINGW}/bin/${hmmm_triple}-clang)")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/${hmmm_triple}-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/${hmmm_triple}-clang++")
set(CMAKE_ASM_COMPILER "${LLVM_MINGW}/bin/${hmmm_triple}-clang")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/${hmmm_triple}-windres")
set(CMAKE_C_COMPILER_TARGET ${hmmm_triple})
set(CMAKE_CXX_COMPILER_TARGET ${hmmm_triple})
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/${hmmm_triple}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# The Linux computer's pkg-config must not answer for Windows.
set(ENV{PKG_CONFIG_LIBDIR} "/nonexistent")
set(ENV{PKG_CONFIG_PATH} "")
# Windows finds headers whatever their capitals; Linux does not, and a few sources spell them
# their own way (case-shims/). tools/make_runtime_kit.py leaves this folder out of the recipe.
set(CMAKE_C_FLAGS_INIT "-idirafter ${CMAKE_CURRENT_LIST_DIR}/case-shims")
set(CMAKE_CXX_FLAGS_INIT "-idirafter ${CMAKE_CURRENT_LIST_DIR}/case-shims")
