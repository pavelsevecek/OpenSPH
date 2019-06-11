#include "system/Process.h"
#include "io/FileSystem.h"
#ifdef SPH_MSVC
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

NAMESPACE_SPH_BEGIN

class ProcessException : public std::exception {
private:
    std::string message;

public:
    ProcessException(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

Process::Process(const Path& path, Array<std::string>&& args) {
    if (!FileSystem::pathExists(path)) {
        throw ProcessException("Path " + path.native() + " does not exist");
    }

#ifdef SPH_MSVC
    MARK_USED(args);
    NOT_IMPLEMENTED;
#else
    pid_t pid = fork();
    if (pid == -1) {
        throw ProcessException("Process fork failed");
    } else if (pid == 0) {
        // child process
        Array<char*> argv;
        std::string fileName = path.native();
        /// \todo hopefully c_str doesn't return pointer to some temporary stuff ...
        argv.push(const_cast<char*>(fileName.c_str()));
        for (std::string& arg : args) {
            argv.push(const_cast<char*>(arg.c_str()));
        }
        argv.push(nullptr);

        execvp(fileName.c_str(), &argv[0]);

        // execl replaces the current process with a new one, so we should never get past the call
        throw ProcessException("Cannot execute file " + fileName);
    } else {
        ASSERT(pid > 0);
        // parent process, save the child pid
        childPid = pid;
    }
#endif
}

void Process::wait() {
#ifdef SPH_MSVC
    NOT_IMPLEMENTED;
#else
    int status;
    pid_t pid;
    do {
        pid = waitpid(childPid, &status, 0);
    } while (pid != childPid && status != -1);
#endif
}

NAMESPACE_SPH_END
