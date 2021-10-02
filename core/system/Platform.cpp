#include "system/Platform.h"
#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include "objects/utility/Algorithm.h"
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

Outcome sendMail(const String& to, const String& from, const String& subject, const String& message) {
#ifndef SPH_WIN
    NOT_IMPLEMENTED; // somehow doesn't work
    FILE* mailpipe = popen("/usr/bin/sendmail -t", "w");
    if (mailpipe == nullptr) {
        return makeFailed("Cannot invoke sendmail");
    }
    fprintf(mailpipe, "To: %s\n", to.toAscii().cstr());
    fprintf(mailpipe, "From: %s\n", from.toAscii().cstr());
    fprintf(mailpipe, "Subject: %s\n\n", subject.toAscii().cstr());
    fwrite(message.toAscii().cstr(), 1, message.size(), mailpipe);
    fwrite(".\n", 1, 2, mailpipe);
    pclose(mailpipe);
    return SUCCESS;
#else
    NOT_IMPLEMENTED;
#endif
}

Outcome showNotification(const String& title, const String& message) {
#ifndef SPH_WIN
    String command = "notify-send \"" + title + "\" \"" + message + "\"";
    if (system(command.toAscii().cstr())) {
        return SUCCESS;
    } else {
        return makeFailed("Command failed");
    }
#else
    NOT_IMPLEMENTED;
#endif
}

Outcome sendPushNotification(const String& key, const String& title, const String& message) {
#ifndef SPH_WIN
    String command =
        format("curl --data 'key={}&title={}&msg={}' https://api.simplepush.io/send > /dev/null 2> /dev/null",
            key,
            title,
            message);
    if (system(command.toAscii().cstr())) {
        return SUCCESS;
    } else {
        return makeFailed("Command failed");
    }
#else
    NOT_IMPLEMENTED;
#endif
}

Expected<String> getGitCommit(const Path& pathToGitRoot, const Size prev) {
    if (!FileSystem::pathExists(pathToGitRoot)) {
        return makeUnexpected<String>("Invalid path");
    }
#ifndef SPH_WIN
    StaticArray<char, 128> buffer;
    String command = "cd " + pathToGitRoot.string() + " && git rev-parse HEAD~" + toString(prev);
    String result;
    FILE* pipe = popen(command.toAscii().cstr(), "r");
    auto f = finally([pipe] { pclose(pipe); });
    if (!pipe) {
        return "";
    }
    while (!feof(pipe)) {
        if (fgets(&buffer[0], buffer.size(), pipe) != NULL) {
            result += String::fromAscii(&buffer[0]);
        }
    }
    // remove \n if in the string
    std::size_t n;
    if ((n = result.find("\n")) != String::npos) {
        result = result.substr(0, n);
    }
    // some sanity checks so that we don't return nonsense
    if (result.size() != 40) {
        return makeUnexpected<String>("Returned git SHA has incorrent length ({})", result.size());
    }

    bool validChars =
        allMatching(result, [](wchar_t c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
    if (!validChars) {
        return makeUnexpected<String>("Returned git SHA contains invalid characters");
    }

    return result;

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
