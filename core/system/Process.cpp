#include "system/Process.h"
#include "io/FileSystem.h"
#include "objects/Exceptions.h"

#ifndef SPH_WIN
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

NAMESPACE_SPH_BEGIN

class ProcessException : public Exception {
public:
    using Exception::Exception;
};

Process::Process(const Path& path, Array<String>&& args) {
    if (!FileSystem::pathExists(path)) {
        throw ProcessException(format("Path '{}' does not exist", path.string()));
    }
#ifndef SPH_WIN
    pid_t pid = fork();
    if (pid == -1) {
        throw ProcessException("Process fork failed");
    } else if (pid == 0) {
        // child process
        Array<CharString> arguments;
        Array<char*> argv;
        String fileName = path.string();
        arguments.push(fileName.toAscii());
        for (String& arg : args) {
            arguments.push(arg.toAscii());
            argv.push(arguments.back());
        }
        argv.push(nullptr);
        execvp(path.native(), &argv[0]);

        // execl replaces the current process with a new one, so we should never get past the call
        throw ProcessException("Cannot execute file " + fileName);
    } else {
        SPH_ASSERT(pid > 0);
        // parent process, save the child pid
        childPid = pid;
    }
#else
    NOT_IMPLEMENTED
#endif
}

void Process::wait() {
#ifndef SPH_WIN
    int status;
    pid_t pid;
    do {
        pid = waitpid(childPid, &status, 0);
    } while (pid != childPid && status != -1);
#else
    NOT_IMPLEMENTED
#endif
}

NAMESPACE_SPH_END
