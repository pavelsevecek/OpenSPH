#pragma once

/// \file Platform.h
/// \brief System functions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"
#include <stdio.h>

NAMESPACE_SPH_BEGIN

class Path;

/// \brief Sends a mail with given message.
Outcome sendMail(const String& to, const String& from, const String& subject, const String& message);

/// \brief Shows a notification using 'notify-send' command.
///
/// The function is non-blocking, the notification disappears on timeout or when user hides it.
Outcome showNotification(const String& title, const String& message);

/// \brief Sends a push notification to an Android device, using SimplePush API.
///
/// Requires installed curl on the system.
/// \param key SimplePush key of the Android device.
/// \param title Title of the notification.
/// \param message Notification message.
Outcome sendPushNotification(const String& key, const String& title, const String& message);

/// \brief Returns git commit hash of the current or older commit as string.
///
/// If the git repository is not found or command fails, returns error message
/// \param pathToGitRoot Path to where the source code is located
/// \param prev Number of commits into the history. For 0, the current commit is returned.
Expected<String> getGitCommit(const Path& pathToGitRoot, const Size prev = 0);

Optional<Float> getCpuUsage();

/// \brief Returns true if the program is running with attached debugger.
bool isDebuggerPresent();

NAMESPACE_SPH_END
