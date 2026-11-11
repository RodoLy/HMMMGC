// What fault_handler.cpp needs from the code of each host system (fault_handler_posix.cpp,
// fault_handler_windows.cpp), besides install().
//
// Lo que fault_handler.cpp necesita del código de cada sistema.
#pragma once

namespace fault_handler {

// Prints the host's call stack, up to `maximum_frames` frames, to stderr.
void print_host_backtrace(int maximum_frames);

}  // namespace fault_handler
