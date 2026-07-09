#ifndef LANEX_CONSTANTS_H
#define LANEX_CONSTANTS_H

// Maximum number of LAN-EX pairs the tester supports.
// Used to size the fixed per-pair arrays; the actual pair count for a run
// comes from the test configuration and must be <= MAX_PAIRS.
constexpr int MAX_PAIRS = 8;

#endif
