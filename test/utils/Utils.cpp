#include "utils/Utils.h"
#include "utils/RecordType.h"

using namespace Sph;

int RecordType::constructedNum = 0;
int RecordType::destructedNum = 0;

Array<std::pair<std::string, int>> skippedTests;

std::mutex globalTestMutex;
