// ChildProcess (child_process.h) with SDL's processes, for the systems other than Windows.

#include "child_process.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

namespace {

class SdlChildProcess final : public ChildProcess {
public:
    SdlChildProcess(SDL_Process* process, SDL_IOStream* output_file) : process_(process), output_file_(output_file) {}

    ~SdlChildProcess() override {
        SDL_DestroyProcess(process_);
        if (output_file_ != nullptr) {
            SDL_CloseIO(output_file_);
        }
    }

    std::string read_output() override {
        SDL_IOStream* output = SDL_GetProcessOutput(process_);
        if (output == nullptr) {
            return {};
        }
        // SDL's pipe does not wait: with nothing to read yet it answers 0 and "not ready".
        char buffer[4096];
        while (true) {
            const size_t read = SDL_ReadIO(output, buffer, sizeof(buffer));
            if (read > 0) {
                return std::string(buffer, read);
            }
            if (SDL_GetIOStatus(output) != SDL_IO_STATUS_NOT_READY) {
                return {};  // the program closed its output
            }
            SDL_Delay(20);
        }
    }

    std::optional<int> exit_code() override {
        int code = 0;
        if (SDL_WaitProcess(process_, false, &code)) {
            return code;
        }
        return std::nullopt;
    }

    void kill() override { SDL_KillProcess(process_, true); }

private:
    SDL_Process* process_;
    SDL_IOStream* output_file_;
};

}  // namespace

std::unique_ptr<ChildProcess> ChildProcess::start(const Options& options, std::string& error) {
    std::vector<const char*> arguments;
    for (const std::string& argument : options.arguments) {
        arguments.push_back(argument.c_str());
    }
    arguments.push_back(nullptr);

    SDL_Environment* environment = SDL_CreateEnvironment(true);
    for (const auto& [name, value] : options.environment) {
        SDL_SetEnvironmentVariable(environment, name.c_str(), value.c_str(), true);
    }
    SDL_IOStream* output_file = nullptr;
    if (!options.output_file.empty()) {
        output_file = SDL_IOFromFile(options.output_file.c_str(), "wb");
        if (output_file == nullptr) {
            error = SDL_GetError();
            SDL_DestroyEnvironment(environment);
            return nullptr;
        }
    }

    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, arguments.data());
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, environment);
    if (!options.working_directory.empty()) {
        SDL_SetStringProperty(properties, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
                              options.working_directory.c_str());
    }
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    if (output_file != nullptr) {
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
        SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, output_file);
    } else {
        SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    }
    SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    SDL_DestroyEnvironment(environment);
    if (process == nullptr) {
        error = SDL_GetError();
        if (output_file != nullptr) {
            SDL_CloseIO(output_file);
        }
        return nullptr;
    }
    return std::make_unique<SdlChildProcess>(process, output_file);
}
