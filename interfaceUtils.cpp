#include "interfaceUtils.h"

namespace InterfaceUtils {
    void initScreen() {
        initscr();
        refresh();
    }

    void printSpacer(int length) {
        std::string outSpacer;
        for(int i = 0; i < length; i++) {
            outSpacer += "-";
        }

        printw(outSpacer.c_str());
    }

    void createNewPage(std::string pageTitle, std::string pageDescription, std::string pageQuestion) {
        clear();
        
        // Get screen dimensions 
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);

        // Print header
        move(0, 0);
        printw("LAN-EX H/F Tester\n");
        printw(pageTitle.c_str());
        move(1, maxCol - 19);
        printw("type 'b' to go back");
        move(2, 0);
        printSpacer(maxCol);

        // Print description
        move(3,0);
        printw(pageDescription.c_str());

        // Print question
        move(maxRow - 3, 0);
        printSpacer(maxCol);
        move(maxRow - 2, 0);
        printw(pageQuestion.c_str());
        move(maxRow - 1, 0);
        printw("input: ");
        refresh();
    }

    void createNewTestMonitorPage(ConfigureTest::testConfiguration *tc) {
        clear();
        
        // Get screen dimensions 
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);

        // Print header
        move(0, 0);
        printw("LAN-EX Tester\nTest in progress");
        move(2, 0);
        printSpacer(maxCol);

        // Print test body
        for(int i = 0; i < tc->numOfPairs; i++) {
            move(3 + 3*i,0);
            std::string line1 = "Pair ";
            line1 += std::to_string(i + 1);
            line1 += ":\t\t\t\tTX\t\tRX\n";
            std::string line2 = tc->serialNumberPairs[i];
            line2 += "\t\t\t-\t\t-\n";

            printw(line1.c_str());
            printw(line2.c_str());
            printSpacer(maxCol);
        }

        // Print progress
        move(maxRow - 2, 0);
        printSpacer(maxCol);
        move(maxRow - 1, 0);
        printw("Progress: 0%%");
        refresh();
    }

    void moveCursorOutOfScreen() {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        move(maxRow -1, maxCol - 1);
        printw(".");
    }

    void updateTxOfPair(int pairN, std::string val) {
        //32
        move(3 + 3*pairN + 1, 32);
        printw("            "); // clear the field so a shorter value can't leave stale chars
        move(3 + 3*pairN + 1, 32);
        printw("%s", val.c_str());
        moveCursorOutOfScreen();
        refresh();
    }

    void updateProgress(std::string val) {
        int maxRow, maxCol;
        getmaxyx(stdscr, maxRow, maxCol);
        move(maxRow -1, 10);
        for(int c = 10; c < maxCol; c++) { printw(" "); } // Clear to end of line
        move(maxRow -1, 10);
        printw("%s", val.c_str());
        moveCursorOutOfScreen();
        refresh();
    }

    void updateRxOfPair(int pairN, std::string val) {
        //48
        move(3 + 3*pairN + 1, 48);
        printw("            "); // clear the field so a shorter value can't leave stale chars
        move(3 + 3*pairN + 1, 48);
        printw("%s", val.c_str());
        moveCursorOutOfScreen();
        refresh();
    }

    void getStringFromCin(std::string &str) {
        char inBuff[1024];
        getstr(inBuff);
        str = inBuff;
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