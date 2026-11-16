# Builds for the Macs with Intel processors on a Mac with Apple silicon (the package workflow's
# runner), with Apple's clang. The processor is said here, not left to CMake to infer from
# CMAKE_OSX_ARCHITECTURES, so that the runtime's x86-64 options and aurora's choice of Dawn's
# package follow it, as WiiCompiled's own file for this does.
#
# Compila para las Mac con Intel en una Mac con Apple silicon.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "the processor of the Macs it is for" FORCE)
