#ifndef PING_EXECUTOR
#define PING_EXECUTOR

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <string.h>
#include <vector>
#include "interfaceUtils.h"

namespace PingExecutor {
    void pingAllTestWindow(int numOfPairs, std::vector<std::string> *ips);
}

#endif