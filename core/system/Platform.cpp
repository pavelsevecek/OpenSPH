#include "system/Platform.h"
#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Finally.h"
#include <fcntl.h>
#include <string.h>

#ifndef SPH_WIN
#include <libgen.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

NAMESPACE_SPH_BEGIN

Outcome sendMail(const std::string& to,
    const std::string& from,
    const std::string& subject,
    const std::string& message) {
#ifndef SPH_WIN
    NOT_IMPLEMENTED; // somehow doesn't work
    FILE* mailpipe = popen("/usr/bin/sendmail -t", "w");
    if (mailpipe == nullptr) {
        return makeFailed("Cannot invoke sendmail");
    }
    fprintf(mailpipe, "To: %s\n", to.c_str());
    fprintf(mailpipe, "From: %s\n", from.c_str());
    fprintf(mailpipe, "Subject: %s\n\n", subject.c_str());
    fwrite(message.c_str(), 1, message.size(), mailpipe);
    fwrite(".\n", 1, 2, mailpipe);
    pclose(mailpipe);
    return SUCCESS;
#else
    NOT_IMPLEMENTED;
#endif
}

Outcome showNotification(const std::string& title, const std::string& message) {
#ifndef SPH_WIN
    std::string command = "notify-send \"" + title + "\" \"" + message + "\"";
    if (system(command.c_str())) {
        return SUCCESS;
    } else {
        return makeFailed("Command failed");
    }
#else
    NOT_IMPLEMENTED;
#endif
}

Outcome sendPushNotification(const std::string& key, const std::string& title, const std::string& message) {
#ifndef SPH_WIN
    std::string command = "curl --data 'key=" + key + "&title=" + title + "&msg=" + message +
                          "' https://api.simplepush.io/send > /dev/null 2> /dev/null";
    if (system(command.c_str())) {
        return SUCCESS;
    } else {
        return makeFailed("Command failed");
    }
#else
    NOT_IMPLEMENTED;
#endif
}

Expected<std::string> getGitCommit(const Path& pathToGitRoot, const Size prev) {
    if (!FileSystem::pathExists(pathToGitRoot)) {
        return makeUnexpected<std::string>("Invalid path");
    }
#ifndef SPH_WIN
    StaticArray<char, 128> buffer;
    std::string command = "cd " + pathToGitRoot.native() + " && git rev-parse HEAD~" + std::to_string(prev);
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    auto f = finally([pipe] { pclose(pipe); });
    if (!pipe) {
        return "";
    }
    while (!feof(pipe)) {
        if (fgets(&buffer[0], buffer.size(), pipe) != NULL) {
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
#else
    NOT_IMPLEMENTED;
#endif
}

#ifndef SPH_WIN
class CpuUsage {
private:
    clock_t lastCpu;
    clock_t lastSysCpu;
    clock_t lastUserCpu;
    Size numProcessors;

public:
    CpuUsage() {
        FILE* file;
        struct tms timeSample;
        char line[128];

        lastCpu = times(&timeSample);
        lastSysCpu = timeSample.tms_stime;
        lastUserCpu = timeSample.tms_utime;

        file = fopen("/proc/cpuinfo", "r");
        numProcessors = 0;
        while (fgets(line, 128, file) != NULL) {
            if (strncmp(line, "processor", 9) == 0)
                numProcessors++;
        }
        fclose(file);
    }

    Optional<Float> getUsage() {
        tms timeSample;
        clock_t now;
        Optional<Float> usage;

        now = times(&timeSample);
        if (now <= lastCpu || timeSample.tms_stime < lastSysCpu || timeSample.tms_utime < lastUserCpu) {
            // Overflow detection. Just skip this value.
            usage = NOTHING;
        } else {
            usage = (timeSample.tms_stime - lastSysCpu) + (timeSample.tms_utime - lastUserCpu);
            usage.value() /= (now - lastCpu);
            usage.value() /= numProcessors;
        }
        lastCpu = now;
        lastSysCpu = timeSample.tms_stime;
        lastUserCpu = timeSample.tms_utime;

        return usage;
    }
};

#else
class CpuUsage {
public:
    Optional<Float> getUsage() {
        return NOTHING;
    }
};

#endif

Optional<Float> getCpuUsage() {
    static CpuUsage cpu;
    return cpu.getUsage();
}

bool isDebuggerPresent() {
#ifndef SPH_WIN
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
#else
    return IsDebuggerPresent();
#endif
}

NAMESPACE_SPH_END
