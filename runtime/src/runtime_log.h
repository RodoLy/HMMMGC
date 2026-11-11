// Small printf-style log for the native runtime: one line per message, with the area
// of the runtime that wrote it ("memory", "dsp", ...).
//
// Registro pequeño estilo printf del runtime nativo: una línea por mensaje, con el área
// del runtime que la escribió.
#pragma once

#include <cstdarg>
#include <cstdio>

namespace runtime_log {

#if defined(__GNUC__) || defined(__clang__)
#define RUNTIME_LOG_PRINTF_FORMAT(format_index, first_argument) \
    __attribute__((format(printf, format_index, first_argument)))
#else
#define RUNTIME_LOG_PRINTF_FORMAT(format_index, first_argument)
#endif

inline void write(const char* level, const char* area, const char* format, va_list arguments) {
    std::fprintf(stderr, "[%s] %s: ", level, area);
    std::vfprintf(stderr, format, arguments);
    std::fputc('\n', stderr);
}

RUNTIME_LOG_PRINTF_FORMAT(2, 3) inline void info(const char* area, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    write("info", area, format, arguments);
    va_end(arguments);
}

RUNTIME_LOG_PRINTF_FORMAT(2, 3) inline void warning(const char* area, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    write("warning", area, format, arguments);
    va_end(arguments);
}

RUNTIME_LOG_PRINTF_FORMAT(2, 3) inline void error(const char* area, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    write("error", area, format, arguments);
    va_end(arguments);
}

}  // namespace runtime_log
