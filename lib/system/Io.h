#pragma once

#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

static Outcome sendMail(const std::string& to,
    const std::string& from,
    const std::string& subject,
    const std::string& message) {
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

NAMESPACE_SPH_END
