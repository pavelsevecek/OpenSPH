#pragma once

/// \file Platform.h
/// \brief System functions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"
#include <stdio.h>

NAMESPACE_SPH_BEGIN

class Path;

/// Sends a mail with given message.
Outcome sendMail(const std::string& to,
    const std::string& from,
    const std::string& subject,
    const std::string& message);

/// Shows a notification using 'notify-send' command. The function is non-blocking, the notification
/// disappears on timeout or when user hides it.
Outcome showNotification(const std::string& title, const std::string& message);

/// Returns current git commit hash as string. If the git repository is not found or command fails, returns
/// empty string.
Expected<std::string> getGitCommit(const Path& pathToGitRoot);

/// Returns true if the program is running with attached debugger.
bool isDebuggerPresent();

NAMESPACE_SPH_END
