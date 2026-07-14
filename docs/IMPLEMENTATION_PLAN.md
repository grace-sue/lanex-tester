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
| Drop → verdict | **Any drop = FAIL.** A pair fails on its first connection drop — no threshold. (A drop marks the cycle FAILED, which `failCount` already captures, so the verdict is just `failCount == 0`.) The **number of cycles the pair had no working link** — dropped mid-stream *or* couldn't connect after retries — is recorded (`dropCycles`) and shown on screen and in the report, for visibility (not for the verdict). | The link dropping at all means the unit isn't reliable; the count tells the operator *how* flaky. |
| Protocol | **TCP throughout.** Phase B adds `-b 10m` to cap the rate. | No UDP/JSON/`Retr` parsing needed; TCP naturally finds max throughput in Phase A. |
| Bidirectional | Phase B uses `iperf3 --bidir`. Phase A stays sequential (both directions one after another). | Literal read of the requirement. |
| Pass/Fail scope | **Scored per pair, independently.** Each pair gets its own verdict; one pair failing has no effect on the others. There is no single cycle-wide or run-wide gate — only a rollup count (e.g. "4 / 5 pairs passed"). | On a bench testing several units at once, one flaky pair must not fail the good units tested alongside it. |
| A pair's verdict | A pair **PASSES** if, across every cycle: (a) it met the `TX`/`RX` throughput targets **in Phase A**, and (b) it never dropped. Otherwise **FAIL**. Both reduce to `failCount == 0`. | Accountable per-unit result. |
| What each phase judges | **Phase A** = throughput test → judged against `TX`/`RX` thresholds. **Phase B** = capped soak → **judged only on connection drops**, not throughput (its job is to expose a link that fails under sustained parallel load). | Separates "is it fast enough?" from "does it stay connected under load?". |
| Stop key | ncurses **non-blocking** `getch()` (`nodelay`), poll for `q`. | Current `getch()` is blocking; the loop must watch for a keypress. |

Open values chosen as defaults (tunable in config): `soakDuration = 30 s`, `retries = 3`. Any drop fails — there is no drop-count threshold.

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

**Recording a drop:** `markDropCycle` flags the cycle as one with **no working link** — a real
mid-stream drop **or** a measurement that couldn't start after retries (a persistently down pair
would otherwise only ever count its first cycle). The flag is set once per cycle (even across both
phases); the count is **committed in `evaluateAndTally` only when the cycle completes**, so a cycle
discarded by `q` can't run `dropCycles` ahead of `cyclesCompleted`. The live **Drops** cell shows the
provisional count immediately. `dropLog` (real drops) and `errorLog` (couldn't-connect) still record
each event separately for the report. The flagged cycle is also marked FAILED — that (not the count)
decides the verdict.

**Retries knob:**
- `retries` — re-attempts of a measurement that **couldn't start** (transient setup error). Does **not**
  apply to connection drops — a real drop fails the pair immediately, with no count or threshold.

---

## 4. Config & constants  *(Milestone 1)*

**`config/targetBandwidth.conf`** — add:
```
F->H:90               # Field->Head target (Mbps), Phase A forward direction
H->F:190              # Head->Field target (Mbps), Phase A reverse direction
phaseDuration:5      # seconds per direction, Phase A (throughput, judged vs F->H/H->F)
soakDuration:30       # seconds, Phase B (capped soak, judged on connection drops only)
soakCap:10            # Mbps per pair, Phase B
retries:3             # immediate auto-retry attempts on a failed measurement
```

**`serverConfigurationLoader.h/.cpp`** — extend `ServerConfiguration` with
`phaseDuration, soakDuration, soakCap, retries`; parse the new keys
(same pattern as the existing `H->F:`/`F->H:`/`duration:` block).

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
    int   dropCycles[MAX_PAIRS] = {0};        // number of cycles the pair dropped in (any drop = fail)
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
  scored** (only its connection drops matter). No cross-pair gate: a pair's result never depends on
  another pair. A pair fails a cycle if it missed the Phase A throughput target **or** dropped — both
  bump `failCount`, so the final verdict is simply `failCount == 0`.

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

**`retries`** only recovers a *failed-to-start* measurement; a real connection drop fails the pair
immediately, with no count or threshold.

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

## 8. Terminal UI  *(Milestone 5 — done)*

**`interfaceUtils.cpp/.h`** — brought the live screen to the approved mockups
(`docs/mockups/lanex-ui-mockups.html`), adapted to the binary-drop model:

- Color via ncurses `start_color` + `use_default_colors`: teal = running/active, green = done/pass,
  amber = retry, red = fail, dim = waiting/muted (the "at-a-glance" cue from the mockups).
- `beginTestMonitor(MonitorInfo)`: draws the full static frame — title bar (LIVE badge + `press Q`
  hint), meta line (operator · config · cycle · elapsed clock), phase-label row, a per-pair table, a
  countdown/soak progress bar, and a running-totals line. Records the run start once so the elapsed
  clock spans the whole run.
- **Table columns:** `Pair · Serial · TX Mbps · RX Mbps · Drops · Status`. **Drops** is the running
  count of cycles the pair dropped in (dim `0` when healthy, red when non-zero); **Status** goes red
  `FAIL` on a drop. Numeric rate cells are colored green/red vs target in Phase A.
- Updaters: `setCycle`, `refreshElapsed`, `setPhaseLine`, `updatePairRate(pair, isRx, val, color)`,
  `updatePairStatus(pair, PairStatus)`, `updatePairRetry(pair, attempt, max)`,
  `drawProgressBar(label, pct, subtext)` (ASCII bar — robust on the rig), `setTotals(done, failed)`.
- **Retry feedback:** a setup-error retry runs inside the measurement worker thread, so it can't touch
  ncurses directly. The worker publishes its attempt number via `testData::retryAttempt[pair]`
  (`std::atomic<int>`); the UI pump reads it each tick and paints an amber `retry N/M` in the Status
  cell, restoring `running` once the retry resolves.
- `bool pollStopKey()` — non-blocking `q` check for the update loops (already in place from M3).

*Deferred:* a dedicated amber `showError` panel and a redesigned `showSummary` result screen are
folded into M6 (reports); the connectivity/ping screen keeps its existing simple page for now.

---

## 9. Reports  *(Milestone 6 — done)*

**`reportGenerator.cpp/.h`** — driven by `RunSummary`:

- **`buildRunSummaryText(tc, sum, &passed)`** — the compact per-pair verdict text, shared by the
  on-screen result (`main.cpp`) and the summary file so they can never disagree.
- **`saveSummaryReport(tc, sum)`** — writes the summary file
  (`reports/<config>_<serial|switch>_<YYYYMMDD_HHMMSS>.txt` — the run timestamp keeps every run's
  file unique instead of overwriting) and appends it to `reports/allReports.txt`. Contents: accountable header (operator, config,
  **start → end time**, cycles completed), a **per-pair verdict** (peak TX/RX, `drops: N` count,
  PASS/FAIL), a **rollup line** ("1 / 3 pairs passed"), and **Connection drops / Errors** sections
  dumping `dropLog` + `errorLog` so failures are documented. No single all-or-nothing verdict.
- Each pair's verdict reuses `LANEXTest::pairPassed` (`failCount == 0`); any throughput miss or drop
  fails it — the rule stays in one place.
- **Eng report:** unchanged raw iperf logs (last cycle); filename now shares the `reportBaseName`
  helper (fixes the old switch-index out-of-range path). Log *capping* is still M7.

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
