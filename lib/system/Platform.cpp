#include "system/Platform.h"
#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Finally.h"
#include <fcntl.h>

#ifdef SPH_MSVC
#include <Windows.h>
#include <ctime>
#else
#include <libgen.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/vtimes.h>
#include <unistd.h>
#endif

NAMESPACE_SPH_BEGIN

Expected<Path> getExecutablePath() {
#ifdef SPH_MSVC
    NOT_IMPLEMENTED;
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        Path path(std::string(result, count));
        return path.parentPath();
    } else {
        return makeUnexpected<Path>("Unknown error");
    }
#endif
}

Outcome showNotification(const std::string& title, const std::string& message) {
    std::string command = "notify-send \"" + title + "\" \"" + message + "\"";
    if (system(command.c_str())) {
        return SUCCESS;
    } else {
        return "Command failed";
    }
}

Outcome sendPushNotification(const std::string& key, const std::string& title, const std::string& message) {
    std::string command = "curl --data 'key=" + key + "&title=" + title + "&msg=" + message +
                          "' https://api.simplepush.io/send > /dev/null 2> /dev/null";
    if (system(command.c_str())) {
        return SUCCESS;
    } else {
        return "Command failed";
    }
}

Expected<std::string> getGitCommit(const Path& pathToGitRoot, const Size prev) {
    if (!FileSystem::pathExists(pathToGitRoot)) {
        return makeUnexpected<std::string>("Invalid path");
    }

#ifdef SPH_MSVC
    MARK_USED(prev);
    NOT_IMPLEMENTED;
#else
    StaticArray<char, 128> buffer;
    std::string command = "cd " + pathToGitRoot.native() + " && git rev-parse HEAD~" + std::to_string(prev);
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
#endif
}

#ifdef SPH_MSVC
#else
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
#endif

Optional<Float> getCpuUsage() {
#ifdef SPH_MSVC
    NOT_IMPLEMENTED;
#else
    static CpuUsage cpu;
    return cpu.getUsage();
#endif
}

bool isDebuggerPresent() {
#ifdef SPH_MSVC
    return IsDebuggerPresent() != FALSE;
#else

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
#endif
}

NAMESPACE_SPH_END
