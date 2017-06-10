#include "system/Platform.h"
#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Finally.h"
#include <fcntl.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

NAMESPACE_SPH_BEGIN

Outcome sendMail(const std::string& to,
    const std::string& from,
    const std::string& subject,
    const std::string& message) {
    NOT_IMPLEMENTED; // somehow doesn't work
    FILE* mailpipe = popen("/usr/bin/sendmail -t", "w");
    if (mailpipe == nullptr) {
        return "Cannot invoke sendmail";
    }
    fprintf(mailpipe, "To: %s\n", to.c_str());
    fprintf(mailpipe, "From: %s\n", from.c_str());
    fprintf(mailpipe, "Subject: %s\n\n", subject.c_str());
    fwrite(message.c_str(), 1, message.size(), mailpipe);
    fwrite(".\n", 1, 2, mailpipe);
    pclose(mailpipe);
    return SUCCESS;
}

/// Shows a notification using 'notify-send' command. The function is non-blocking, the notification
/// disappears on timeout or when user hides it.
Outcome showNotification(const std::string& title, const std::string& message) {
    std::string command = "notify-send \"" + title + "\" \"" + message + "\"";
    if (system(command.c_str())) {
        return SUCCESS;
    } else {
        return "Command failed";
    }
}

/// Returns current git commit hash as string. If the git repository is not found or command fails, returns
/// empty string.
Expected<std::string> getGitCommit(const Path& pathToGitRoot) {
    if (!pathExists(pathToGitRoot)) {
        return makeUnexpected<std::string>("Invalid path");
    }
    StaticArray<char, 128> buffer;
    std::string command = "cd " + pathToGitRoot.native() + " && git rev-parse HEAD";
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    auto f = finally([pipe] { pclose(pipe); });
    if (!pipe) {
        return "";
    }
    while (!feof(pipe)) {
        if (fgets(&buffer[0], 128, pipe) != NULL) {
            result += &buffer[0];
        }
    }
    // remove \n if in the string
    std::size_t n;
    if ((n = result.find("\n")) != std::string::npos) {
        result = result.substr(0, n);
    }
    // some sanity checks so that we don't return nonsense
    if (result.size() != 40) {
        return makeUnexpected<std::string>(
            "Returned git SHA has incorrent length (" + std::to_string(result.size()) + ")");
    } else if (result.find_first_not_of("0123456789abcdef") != std::string::npos) {
        return makeUnexpected<std::string>("Returned git SHA contains invalid characters");
    } else {
        return result;
    }
}

bool isDebuggerPresent() {
    char buf[1024];
    bool debuggerPresent = false;

    int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1) {
        return false;
    }

    std::size_t num_read = read(status_fd, buf, sizeof(buf));

    if (num_read > 0) {
        static const char TracerPid[] = "TracerPid:";
        char* tracer_pid;

        buf[num_read] = 0;
        tracer_pid = strstr(buf, TracerPid);
        if (tracer_pid) {
            debuggerPresent = atoi(tracer_pid + sizeof(TracerPid) - 1) != 0;
        }
    }
    return debuggerPresent;
}

NAMESPACE_SPH_END
