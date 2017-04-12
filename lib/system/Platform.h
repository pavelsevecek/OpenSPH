#pragma once

#include "objects/containers/StaticArray.h"
#include "objects/wrappers/Outcome.h"
#include <memory>
#include <stdio.h>

NAMESPACE_SPH_BEGIN

inline Outcome sendMail(const std::string& to,
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
    return true;
}

/// Shows a notification using 'notify-send' command. The function is non-blocking, the notification
/// disappears on timeout or when user hides it.
inline Outcome showNotification(const std::string& title, const std::string& message) {
    std::string command = "notify-send \"" + title + "\" \"" + message + "\"";
    if (system(command.c_str())) {
        return true;
    } else {
        return "Command failed";
    }
}

/// Returns current git commit hash as string. If the git repository is not found or command fails, returns
/// empty string.
inline std::string getGitCommit(const std::string& pathToGitRoot) {
    StaticArray<char, 128> buffer;
    std::string command = "cd " + pathToGitRoot + " && git rev-parse HEAD";
    std::string result;
    /// \todo unique_ptr with deleter
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return "";
    }
    while (!feof(pipe.get())) {
        if (fgets(&buffer[0], 128, pipe.get()) != NULL)
            result += &buffer[0];
    }
    return result;
}

NAMESPACE_SPH_END
