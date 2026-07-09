# Implementation Plan — LAN-EX Tester v2

Turns the current linear tester (`main → configure → ping → test → report`) into a
continuous, two-phase, connection-drop-aware tester per the new requirements.

Status: **draft for review — not yet implemented.**

---

## 1. Requirements this addresses

From the requirements doc (§2.1 Required):

- Operator-friendly, accountable results (operator initials, date/time, pass/fail, measured summary).
- As many pairs as possible, **minimum 5**.
- Determine **max throughput** on each pair, **both directions one after another, 10 s each**; then run
  **all pairs in parallel** with iperf3 **capped at 10 Mbps each**.
- Run in an **infinite loop** once started, until the operator presses a key to stop — **then** generate reports.
- **Retry mechanism** on error; errors shown on the interface **and** documented in reports.
- **Detect and record connection drops** per pair (see §3).
- Send **bidirectional** network traffic.

§2.2 Desired: serial auto-scan (future), easy porting to other PCs / jigs (constants + `config/`).

---

## 2. Finalized design decisions

| Topic | Decision | Rationale |
|---|---|---|
| "Drops" meaning | **Connection drops only** — a pair losing its link during a test. Not packet loss / retransmits. | Matches the operator's real concern; a flaky link is what fails a unit. |
| Drop → verdict | **Any drop = FAIL.** A pair fails on its **first** connection drop — zero tolerance. (`maxConnDrops = 0`; the knob is kept configurable but defaults to 0.) | The link dropping at all means the unit isn't reliable. |
| Protocol | **TCP throughout.** Phase B adds `-b 10m` to cap the rate. | No UDP/JSON/`Retr` parsing needed; TCP naturally finds max throughput in Phase A. |
| Bidirectional | Phase B uses `iperf3 --bidir`. Phase A stays sequential (both directions one after another). | Literal read of the requirement. |
| Pass/Fail scope | **Scored per pair, independently.** Each pair gets its own verdict; one pair failing has no effect on the others. There is no single cycle-wide or run-wide gate — only a rollup count (e.g. "4 / 5 pairs passed"). | On a bench testing several units at once, one flaky pair must not fail the good units tested alongside it. |
| A pair's verdict | A pair **PASSES** if, across every cycle: (a) it met the `TX`/`RX` throughput targets **in Phase A**, and (b) it had **zero connection drops** (`connectionDrops ≤ maxConnDrops`, default 0). Otherwise **FAIL**. | Accountable per-unit result. |
| What each phase judges | **Phase A** = throughput test → judged against `TX`/`RX` thresholds. **Phase B** = capped soak → **judged only on connection drops**, not throughput (its job is to expose a link that fails under sustained parallel load). | Separates "is it fast enough?" from "does it stay connected under load?". |
| Stop key | ncurses **non-blocking** `getch()` (`nodelay`), poll for `q`. | Current `getch()` is blocking; the loop must watch for a keypress. |

Open values chosen as defaults (tunable in config): `soakDuration = 30 s`, `maxConnDrops = 0` (any drop fails), `retries = 3`.

---

## 3. Connection-drop model

A failed iperf run has **two kinds of cause**, and they are handled differently — this is what makes
"if drop, don't retry" work:

- **Connection drop** — the client *connected and started streaming*, then the stream **died mid-test**
  (`Connection reset by peer`, `control socket has closed`, or interval data that stops before the
  final summary). → **Record it and FAIL the pair — no retry.** The run continues for the other pairs
  and next cycle.
- **Couldn't start (transient setup error)** — the client **never connected at all** (ssh error,
  local server didn't bind, host briefly unreachable *before* any data flowed). → **Retry** up to
  `retries`. This is *not* a connection drop; it doesn't count against the pair unless retries are
  exhausted, in which case it's logged as an error.

**Telling them apart:** iperf3 prints `Connecting to host…` then `connected to…` then per-second
intervals. **Got "connected" + interval data, then it ended early ⇒ connection drop** (fail, no retry).
**Never got "connected" ⇒ setup error** (retry). The pre-test ping already confirms reachability, so a
mid-stream death is a genuine link drop, not a cold-start hiccup.

**Recording a drop:** increment `connectionDrops[pair]`, append a timestamped line to `dropLog`
(`"cycle 12 · pair 3 · connection lost @14:22:07"`), show it on screen.

**Two separate knobs:**
- `retries` — re-attempts of a measurement that **couldn't start** (transient setup error). Does **not**
  apply to connection drops.
- `maxConnDrops` — total connection drops a pair may accumulate before FAIL (default **0** — the first
  drop fails the pair).

---

## 4. Config & constants  *(Milestone 1)*

**`config/targetBandwidth.conf`** — add:
```
TX:90
RX:190
phaseDuration:5      # seconds per direction, Phase A (throughput, judged vs TX/RX)
soakDuration:30       # seconds, Phase B (capped soak, judged on connection drops only)
soakCap:10            # Mbps per pair, Phase B
maxConnDrops:0        # drops allowed before FAIL; 0 = any connection drop fails the pair
retries:3             # immediate auto-retry attempts on a failed measurement
```

**`serverConfigurationLoader.h/.cpp`** — extend `ServerConfiguration` with
`phaseDuration, soakDuration, soakCap, maxConnDrops, retries`; parse the new keys
(same pattern as the existing `RX:`/`TX:`/`duration:` block).

**New `constants.h`** — `constexpr int MAX_PAIRS = 8;` to replace the literal `8`
hardcoded in `testData.h`, `test.cpp`, etc. *(Portability: IPs are already externalized in `config/`.)*

---

## 5. Data model  *(Milestone 1)*

**`testData.h`** — split "current cycle" from "whole run":

```cpp
enum PairStatus { WAITING, RUNNING, DONE, RETRYING, FAILED };

struct PairCycle {                 // reset each cycle
    float txRate = -1, rxRate = -1;      // Mbps, Phase A peaks
    float soakTx = -1, soakRx = -1;      // Mbps, Phase B
    PairStatus status = WAITING;
};

struct RunSummary {                // lives for the whole run
    int   cyclesCompleted = 0;
    int   passCount[MAX_PAIRS] = {0};
    int   failCount[MAX_PAIRS] = {0};
    int   connectionDrops[MAX_PAIRS] = {0};   // times each pair lost its link
    float peakTx[MAX_PAIRS] = {0}, peakRx[MAX_PAIRS] = {0};
    std::vector<std::string> dropLog;    // timestamped connection-drop events
    std::vector<std::string> errorLog;   // other errors
    std::string startTime, endTime;
    std::string engLog;                  // capped raw iperf logs
};
```

Keep the live `currentRxRate/currentTxRate` arrays for second-by-second UI updates
(each pair's thread owns its own index — the existing thread-safety model holds).

---

## 6. iperf measurement  *(Milestone 2)*

**`iperfExecutor.h/.cpp`:**

1. Generalize `startRemoteIperf3Client` to take `bandwidthCap` (0 = uncapped, else `-b <cap>m`)
   and `bidir` (append `--bidir`).
2. Return an explicit **result code** (completed / connection-drop / error) instead of the current
   `-2` sentinel in the rate array, so retry logic can act cleanly.
3. Treat "stream ended without a final summary line" as a **connection drop**.
4. *(No `Retr`, UDP, or JSON parsing — removed from the earlier draft.)*

This also cleans up the fragile `-2` signaling and the commented-out debug block in `iperfExecutor.cpp`.

---

## 7. Test engine — two phases, infinite loop, retry  *(Milestone 3–4)*

**`test.cpp/.h`** — replace `startTest`/`startTestRound` with:

```cpp
void runContinuous(tc, serverConf, RunSummary& sum);   // the main loop
```

- **`runContinuous`:** capture `startTime`; `nodelay(stdscr, TRUE)`;
  `while (!stopRequested()) { cycle++; phaseA(); if(stop) break; phaseB(); evaluateAndTally(sum); }`;
  capture `endTime`. `stopRequested()` polls `getch()=='q'`.
- **`phaseA` (sequential):** for each pair, each direction, run one `phaseDuration`-second measurement.
  If it **connects then drops** → record the drop, mark the pair FAIL, **move on** (no retry). If it
  **couldn't start** → `withRetry(...)`. Update the pair's live row + countdown bar; poll for stop
  between pairs.
- **`phaseB` (parallel soak):** launch all pairs at once as TCP `--bidir -b <cap>m` threads for
  `soakDuration` (30 s); live-update; join. If a pair **connects then drops** mid-soak → record it and
  mark the pair FAIL; its thread ends while the others keep soaking (no reconnect). If it **couldn't
  start** → `withRetry(...)`. **Throughput here is shown live but not scored** — Phase B only checks
  that the connection stays up.
- **`withRetry(attempt, retries)`:** used **only for the "couldn't start" case**. Run `attempt`; on a
  transient start-up error → show the error panel, retry up to `retries`; if still failing → log the
  error, leave the pair for this cycle, and **keep the run going** (never abort the whole run).
  A genuine connection drop is **not** routed through here — it fails the pair directly.
- **`evaluateAndTally`:** score **each pair on its own** — compare its **Phase A** rates vs the `TX`/`RX`
  targets and bump that pair's `passCount`/`failCount` and `peakTx/Rx`. **Phase B throughput is not
  scored** (only its connection drops are counted). No cross-pair gate: a pair's result never depends on
  another pair. A pair's final verdict is FAIL if it missed the Phase A throughput target in any cycle
  **or** `connectionDrops > maxConnDrops`.

Resolves the existing `// TODO: Kill threads if there's an error` in `test.cpp`.

### 7a. Retries are bounded and non-blocking

`retries` re-attempts a measurement that **couldn't start** (a transient setup error — see §3). It does
**not** apply to connection drops, which fail the pair immediately. Two rules keep it from ever hanging
the tester:

- **Every attempt has a timeout.** Use `ssh -o BatchMode=yes -o ConnectTimeout=<n>` and iperf3
  `--connect-timeout`, so a dead pair **fails fast** instead of waiting forever. Worst case a struggling
  step costs ≈ `retries × attempt-timeout`, then it's done. Without this we hit the exact bug the
  [README](../README.md) warns about — ssh silently blocking on a password prompt while the screen
  freezes on `tmp`.
- **The stop key keeps working.** `q` is polled between attempts, so retrying never locks the operator out.

The two knobs do unrelated jobs: **`retries`** recovers a *failed-to-start* measurement; **`maxConnDrops`**
(default **0**) is the *connection-drop* tolerance — the first real drop fails the pair regardless of
`retries`.

### 7b. Phase B soak flow (concurrent)

Phase B runs all pairs **at the same time**, each in its own thread, so one pair failing never pauses the
healthy pairs. Per pair:

1. **Couldn't start the soak** (transient server/ssh glitch) → `withRetry` up to `retries`.
2. **Dropped mid-soak** (the case Phase B exists to catch) → **record the drop, mark the pair FAIL, end
   its thread.** No reconnect, no resume — the drop already decided the verdict. The other pairs keep
   soaking for the rest of the window.

Phase B ends on a **30 s wall clock**, so its duration stays bounded (~30 s + a small tail for the
slowest pair); `q` is polled throughout.

**Timeline example — 5 pairs, 30 s:**
```
t=0s    all 5 pairs start soaking together
t=11s   pair 3 connection drops  → record drop, pair 3 = FAIL, thread ends
        (pairs 1,2,4,5 keep soaking, unaffected)
t=30s   wall clock hits → remaining pairs stop → Phase B done
        result: pair 3 FAIL (1 drop), pairs 1,2,4,5 PASS
```

---

## 8. Terminal UI  *(Milestone 5)*

**`interfaceUtils.cpp/.h`** — bring the live screen to the approved mockups
(`docs/mockups/lanex-ui-mockups.html`):

- `createNewTestMonitorPage`: top status line (LIVE badge + `press Q` hint), meta line
  (operator · config · cycle · elapsed), and **Drops + Status** columns (Drops = connection-drop count).
- New updaters: `updateDrops`, `updateStatus`, `updateCycle`, `updateElapsed`, `setPhaseLabel`.
- `drawBar(label, pct, subtext)` — block-char progress/countdown bar (Phase A countdown, Phase B
  soak timer, ping progress).
- `showError(pair, reason, attempt, retries)` — amber error panel.
- `showSummary(RunSummary&)` — final PASS/FAIL result screen.
- `bool pollStop()` — non-blocking `q` check for the update loops.

---

## 9. Reports  *(Milestone 6)*

**`reportGenerator.cpp/.h`** — take `RunSummary`:

- **Summary report:** operator, **start → end time**, cycles completed, a **per-pair verdict** for each
  pair (peak TX/RX, connection-drop count, PASS/FAIL), and a **rollup line** ("4 / 5 pairs passed").
  There is no single all-or-nothing verdict — the per-pair results are the authoritative outcome.
- **Drop / error section:** dump `dropLog` + `errorLog` so failures are **documented in the report**.
- **Eng report:** capped raw iperf logs.
- Each pair's verdict = met throughput in every cycle it ran **and** `connectionDrops ≤ maxConnDrops`.

---

## 10. Loose ends  *(Milestone 7)*

- **Unbounded memory:** don't append every raw iperf line forever (current `iperfExecutor.cpp` would
  leak over a long soak). Keep full raw logs only for the current cycle; carry aggregates + a **capped**
  drop/error log across cycles.
- **Makefile:** confirm `-lpthread` / `-lncurses`.
- **`main.cpp`** shrinks to: `init → loadConfig → configure → pingAll → runContinuous(sum) → generateReports(sum) → end`.

---

## 11. Build order (milestones)

1. **Config + data model + constants** (§4–5) — no behavior change, compiles.
2. **iperf: cap + bidir + connection-drop result code** (§6) — verify against a live pair.
3. **Two-phase engine + infinite loop + `q`** (§7).
4. **Retry + drop/error recording** (§7, §3).
5. **UI screens** (§8).
6. **Reports from summary** (§9).
7. **Cleanup + portability + docs** (§10).

Each milestone is independently testable on the rig.
