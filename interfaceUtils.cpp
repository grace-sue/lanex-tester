#include "interfaceUtils.h"
#include <chrono>
#include <clocale>

namespace InterfaceUtils {

    // ---- fixed monitor layout ----
    static const int ROW_TITLE = 0;
    static const int ROW_META  = 2;
    static const int ROW_RULE1 = 3;
    static const int ROW_PHASE = 4;
    static const int ROW_RULE2 = 5;
    static const int ROW_THEAD = 6;
    static const int ROW_PAIR0 = 7;   // pair i renders on ROW_PAIR0 + i

    static const int X_PAIR   = 2;    // column left edges
    static const int X_SERIAL = 7;
    static const int X_TX     = 25;   // numeric columns are right-aligned in their width
    static const int X_RX     = 37;
    static const int X_DROPS  = 48;
    static const int X_STATUS = 57;
    static const int W_SERIAL = 16;
    static const int W_RATE   = 9;
    static const int W_DROPS  = 6;
    static const int W_STATUS = 14;

    static const int BAR_W = 20;

    // ---- monitor session state ----
    static struct {
        MonitorInfo info;
        std::chrono::steady_clock::time_point runStart;
        bool runStartSet = false;
        int  cycle = 1;
        int  done = 0, failed = 0;
    } M;

    // Turn a color-pair id (or CP_DIM) into ncurses attributes and back.
    static void colorOn(int cp, bool bold) {
        if(cp == CP_DIM)      attron(A_DIM);
        else if(cp > 0)       attron(COLOR_PAIR(cp));
        if(bold)              attron(A_BOLD);
    }
    static void colorOff(int cp, bool bold) {
        if(cp == CP_DIM)      attroff(A_DIM);
        else if(cp > 0)       attroff(COLOR_PAIR(cp));
        if(bold)              attroff(A_BOLD);
    }

    // Clear a fixed-width field and write text into it (optionally right-aligned).
    static void putField(int y, int x, int width, const std::string &s,
                         int cp, bool bold, bool rightAlign) {
        move(y, x);
        for(int i = 0; i < width; i++) addch(' ');
        std::string t = s.size() > (size_t)width ? s.substr(0, width) : s;
        int px = rightAlign ? x + width - (int)t.size() : x;
        colorOn(cp, bold);
        mvprintw(y, px, "%s", t.c_str());
        colorOff(cp, bold);
    }

    static void parkCursor() {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        move(maxRow - 1, maxCol - 1);
    }

    static void hrule(int y) {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        move(y, 0);
        colorOn(CP_DIM, false);
        for(int c = 0; c < maxCol; c++) addch('-');
        colorOff(CP_DIM, false);
    }

    void initScreen() {
        setlocale(LC_ALL, "");
        initscr();
        if(has_colors()) {
            start_color();
            use_default_colors();
            init_pair(CP_TEAL,  COLOR_CYAN,   -1);
            init_pair(CP_GREEN, COLOR_GREEN,  -1);
            init_pair(CP_AMBER, COLOR_YELLOW, -1);
            init_pair(CP_RED,   COLOR_RED,    -1);
        }
        refresh();
    }

    void createNewPage(std::string pageTitle, std::string pageDescription, std::string pageQuestion) {
        clear();

        // Get screen dimensions
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);

        // Print header
        move(0, 0);
        printw("LAN-EX H/F Tester\n");
        printw("%s", pageTitle.c_str());
        move(1, maxCol - 19);
        printw("type 'b' to go back");
        hrule(2);

        // Print description
        move(3, 0);
        printw("%s", pageDescription.c_str());

        // Print question
        hrule(maxRow - 3);
        move(maxRow - 2, 0);
        printw("%s", pageQuestion.c_str());
        move(maxRow - 1, 0);
        printw("input: ");
        refresh();
    }

    void getStringFromCin(std::string &str) {
        char inBuff[1024];
        getstr(inBuff);
        str = inBuff;
    }

    // ---------------- live test monitor ----------------

    static const char* statusText(LANEXTest::PairStatus s) {
        switch(s) {
            case LANEXTest::RUNNING:  return "running";
            case LANEXTest::DONE:     return "done";
            case LANEXTest::RETRYING: return "retry";
            case LANEXTest::FAILED:   return "FAIL";
            default:                  return "waiting";
        }
    }
    static int statusColor(LANEXTest::PairStatus s) {
        switch(s) {
            case LANEXTest::RUNNING:  return CP_TEAL;
            case LANEXTest::DONE:     return CP_GREEN;
            case LANEXTest::RETRYING: return CP_AMBER;
            case LANEXTest::FAILED:   return CP_RED;
            default:                  return CP_DIM;
        }
    }

    void refreshElapsed() {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        int secs = 0;
        if(M.runStartSet) {
            secs = (int)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - M.runStart).count();
        }
        char clock[16];
        snprintf(clock, sizeof(clock), "%02d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);

        // Redraw the whole meta line so field widths never leave stale digits.
        move(ROW_META, 0);
        for(int c = 0; c < maxCol; c++) addch(' ');
        colorOn(CP_DIM, false);
        mvprintw(ROW_META, 2, "Operator ");   colorOff(CP_DIM, false);
        colorOn(0, true);   printw("%s", M.info.operatorInitials.c_str()); colorOff(0, true);
        colorOn(CP_DIM, false); printw("   Config "); colorOff(CP_DIM, false);
        colorOn(0, true);   printw("%s", M.info.configName.c_str()); colorOff(0, true);
        colorOn(CP_DIM, false); printw("   Cycle "); colorOff(CP_DIM, false);
        colorOn(0, true);   printw("%d", M.cycle); colorOff(0, true);
        colorOn(CP_DIM, false); printw("   Elapsed "); colorOff(CP_DIM, false);
        colorOn(CP_TEAL, false); printw("%s", clock); colorOff(CP_TEAL, false);
        parkCursor();
        refresh();
    }

    void setCycle(int cycle) {
        M.cycle = cycle;
        refreshElapsed();
    }

    void setPhaseLine(const std::string &text) {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        move(ROW_PHASE, 0);
        for(int c = 0; c < maxCol; c++) addch(' ');
        colorOn(CP_TEAL, false);
        mvprintw(ROW_PHASE, 2, "%s", text.c_str());
        colorOff(CP_TEAL, false);
        parkCursor();
        refresh();
    }

    void setTotals(int done, int failed) {
        M.done = done;
        M.failed = failed;
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        int y = maxRow - 1;
        move(y, 0);
        for(int c = 0; c < maxCol; c++) addch(' ');
        colorOn(CP_DIM, false);
        mvprintw(y, 2, "Totals  done ");   colorOff(CP_DIM, false);
        colorOn(CP_GREEN, true); printw("%d", done); colorOff(CP_GREEN, true);
        colorOn(CP_DIM, false); printw("  fail "); colorOff(CP_DIM, false);
        colorOn(failed > 0 ? CP_RED : 0, true); printw("%d", failed); colorOff(failed > 0 ? CP_RED : 0, true);
        colorOn(CP_DIM, false);
        printw("      target >= %d / %d Mbps", (int)M.info.txTarget, (int)M.info.rxTarget);
        colorOff(CP_DIM, false);
        parkCursor();
        refresh();
    }

    void beginTestMonitor(const MonitorInfo &info) {
        M.info = info;
        if(!M.runStartSet) {
            M.runStart = std::chrono::steady_clock::now();
            M.runStartSet = true;
        }
        clear();
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);

        // Title bar: name on the left, LIVE + stop hint on the right.
        colorOn(0, true);
        mvprintw(ROW_TITLE, 2, "LAN-EX H/F Tester");
        colorOff(0, true);
        std::string hint = "  press Q to stop & report";
        int rx = maxCol - (int)hint.size() - 4;
        if(rx < 24) rx = 24;
        colorOn(CP_RED, true);
        mvprintw(ROW_TITLE, rx, "LIVE");
        colorOff(CP_RED, true);
        colorOn(CP_DIM, false);
        printw("%s", hint.c_str());
        colorOff(CP_DIM, false);

        hrule(ROW_RULE1);
        hrule(ROW_RULE2);
        hrule(maxRow - 2);

        // Table header.
        putField(ROW_THEAD, X_PAIR,   4,        "Pair",    CP_DIM, false, false);
        putField(ROW_THEAD, X_SERIAL, W_SERIAL, "Serial",  CP_DIM, false, false);
        putField(ROW_THEAD, X_TX,     W_RATE,   "TX Mbps", CP_DIM, false, true);
        putField(ROW_THEAD, X_RX,     W_RATE,   "RX Mbps", CP_DIM, false, true);
        putField(ROW_THEAD, X_DROPS,  W_DROPS,  "Drops",   CP_DIM, false, true);
        putField(ROW_THEAD, X_STATUS, W_STATUS, "Status",  CP_DIM, false, false);

        // Per-pair rows: serial + placeholders + waiting status.
        for(int i = 0; i < info.numPairs; i++) {
            int y = ROW_PAIR0 + i;
            putField(y, X_PAIR, 4, std::to_string(i + 1), 0, false, false);
            std::string serial = (i < (int)info.serials.size()) ? info.serials[i] : "";
            putField(y, X_SERIAL, W_SERIAL, serial, CP_DIM, false, false);
            putField(y, X_TX, W_RATE, "-", CP_DIM, false, true);
            putField(y, X_RX, W_RATE, "-", CP_DIM, false, true);
            putField(y, X_DROPS, W_DROPS, "0", CP_DIM, false, true);
            updatePairStatus(i, LANEXTest::WAITING);
        }

        setPhaseLine("");
        refreshElapsed();          // draws the meta line (operator/config/cycle/elapsed)
        setTotals(0, 0);           // fresh per-cycle progress counters
        parkCursor();
        refresh();
    }

    void updatePairRate(int pair, bool isRx, const std::string &val, int colorPair) {
        if(pair < 0 || pair >= M.info.numPairs) return;
        int y = ROW_PAIR0 + pair;
        putField(y, isRx ? X_RX : X_TX, W_RATE, val, colorPair, false, true);
        parkCursor();
        refresh();
    }

    void updatePairStatus(int pair, LANEXTest::PairStatus status) {
        if(pair < 0 || pair >= M.info.numPairs) return;
        int y = ROW_PAIR0 + pair;
        bool bold = (status == LANEXTest::FAILED);
        putField(y, X_STATUS, W_STATUS, statusText(status), statusColor(status), bold, false);
        parkCursor();
        refresh();
    }

    void updatePairRetry(int pair, int attempt, int maxAttempts) {
        if(pair < 0 || pair >= M.info.numPairs) return;
        int y = ROW_PAIR0 + pair;
        std::string cell = "retry " + std::to_string(attempt) + "/" + std::to_string(maxAttempts);
        putField(y, X_STATUS, W_STATUS, cell, CP_AMBER, true, false);
        parkCursor();
        refresh();
    }

    void updatePairDrops(int pair, int dropCycles) {
        if(pair < 0 || pair >= M.info.numPairs) return;
        int y = ROW_PAIR0 + pair;
        // Any drop is a failure, so a non-zero count is highlighted red.
        int cp = dropCycles > 0 ? CP_RED : CP_DIM;
        putField(y, X_DROPS, W_DROPS, std::to_string(dropCycles), cp, dropCycles > 0, true);
        parkCursor();
        refresh();
    }

    void drawProgressBar(const std::string &label, int pct, const std::string &subtext) {
        if(pct < 0) pct = 0;
        if(pct > 100) pct = 100;
        int filled = pct * BAR_W / 100;

        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        int y = maxRow - 3;
        move(y, 0);
        for(int c = 0; c < maxCol; c++) addch(' ');

        colorOn(CP_DIM, false);
        mvprintw(y, 2, "%s ", label.c_str());
        colorOff(CP_DIM, false);
        addch('[');
        colorOn(CP_TEAL, false);
        for(int i = 0; i < filled; i++) addch('#');
        colorOff(CP_TEAL, false);
        colorOn(CP_DIM, false);
        for(int i = filled; i < BAR_W; i++) addch('-');
        colorOff(CP_DIM, false);
        addch(']');
        colorOn(CP_DIM, false);
        printw(" %d%%  %s", pct, subtext.c_str());
        colorOff(CP_DIM, false);
        parkCursor();
        refresh();
    }

    // Switch getch() between blocking (menus) and non-blocking (live test loop).
    void setNonBlockingInput(bool nonBlocking) {
        nodelay(stdscr, nonBlocking ? TRUE : FALSE);
    }

    // Drains any pending input; returns true if 'q'/'Q' was seen. Requires non-blocking mode.
    bool pollStopKey() {
        bool stop = false;
        int ch = getch();
        while(ch != ERR) {
            if(ch == 'q' || ch == 'Q') {
                stop = true;
            }
            ch = getch();
        }
        return stop;
    }

    void endScreen() {
        endwin();
    }
}
