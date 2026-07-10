#ifndef INTERFACE_UTILS
#define INTERFACE_UTILS
#include <ncurses.h>
#include <string>
#include <vector>
#include "configureTest.h"
#include "testData.h"

namespace InterfaceUtils {
    // Color pairs used by the live monitor. CP_DIM is rendered with the A_DIM
    // attribute rather than a real color pair, so muted text works on any palette.
    static const int CP_TEAL  = 1;   // running / active
    static const int CP_GREEN = 2;   // done / pass
    static const int CP_AMBER = 3;   // retry / warning
    static const int CP_RED   = 4;   // fail
    static const int CP_DIM   = 5;   // waiting / muted

    // Static context for a monitor session, supplied once per run.
    struct MonitorInfo {
        std::string operatorInitials;
        std::string configName;
        int         numPairs = 0;
        float       txTarget = 0;
        float       rxTarget = 0;
        std::vector<std::string> serials;   // per-pair serial numbers
    };

    void initScreen();
    void endScreen();
    void createNewPage(std::string pageTitle, std::string pageDescription, std::string pageQuestion);
    void getStringFromCin(std::string &str);

    // ---- live test monitor (Milestone 5) ----
    // Draw the full static frame; records the run start on the first call so the
    // elapsed clock spans the whole run, not a single cycle.
    void beginTestMonitor(const MonitorInfo &info);
    void setCycle(int cycle);                                   // meta line cycle #
    void refreshElapsed();                                      // recompute elapsed clock
    void setPhaseLine(const std::string &text);                 // phase description row
    void updatePairRate(int pair, bool isRx, const std::string &val, int colorPair);
    void updatePairStatus(int pair, LANEXTest::PairStatus status);
    void updatePairRetry(int pair, int attempt, int maxAttempts);  // amber "retry N/M" status cell
    void drawProgressBar(const std::string &label, int pct, const std::string &subtext);
    void setTotals(int done, int failed);                       // running done / fail counts

    void setNonBlockingInput(bool nonBlocking);
    bool pollStopKey();   // true if 'q'/'Q' is waiting in the input buffer (non-blocking)
}
#endif
