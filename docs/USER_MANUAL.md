# LAN-EX H/F Tester — Operator Manual

This guide is for the operator running the throughput test. It covers how to start the
tester, enter the setup, read the live screen, and find your results. No technical
background is needed.

---

## 1. What this tool does

The LAN-EX H/F Tester checks that each **Head/Field (H/F) pair** in a set can move data
fast enough and hold a steady connection. It runs continuously — testing every pair over
and over — until you stop it. When you stop, it writes a report showing whether each pair
**PASSED** or **FAILED**, stamped with your initials and the date/time so the result is
accountable.

The test runs in two phases, repeated each cycle:

- **Phase A — Max throughput.** Each pair is tested one at a time, first sending (TX) then
  receiving (RX). Each direction is measured against a target speed.
- **Phase B — Soak.** All pairs run together at a low, capped speed for a while. This phase
  only watches for **dropped connections**.

---

## 2. Before you start

Have these ready:

- **Your initials** (2–4 letters) — recorded on the result.
- **The pair serial numbers**, one per pair, in the form `1234567-7654321`
  (7 digits, a dash, 7 digits).
- **The switch serial numbers** (if the set has switches) — the **last 6 characters of each
  switch's MAC address** (letters and/or digits, e.g. `3A9F2C`).
- All pairs and switches **cabled and powered**.

---

## 3. Starting the tester

From the tester PC, open a terminal in the tester folder and run:

```
make run
```

(or `./LAN-EX-Tester` if it is already built).

The setup wizard starts on **Step 1 / 7**.

> **Tip:** At almost any step you can type **`b`** and press **Enter** to go **back** and fix
> the previous answer. The tester checks every entry and will politely re-ask if something
> looks wrong — you can't accidentally continue with a bad value.

---

## 4. Setup steps

You'll be walked through these, one screen at a time. Type the answer, then press **Enter**.

| Step | Screen | What to enter | Rules |
|------|--------|---------------|-------|
| 1/7 | Operator initials | Your initials | 2–4 letters |
| 2/7 | Number of pairs | How many H/F pairs | 1 to 8 |
| 3/7 | Number of switches | How many switches | 0 to 9 *(skipped automatically if there is only 1 pair)* |
| 4/7 | Pair serial numbers | Each pair's serial | `NNNNNNN-NNNNNNN` — 7 digits, dash, 7 digits |
| 5/7 | Switch serial numbers | Each switch's serial | 6 letters/digits (last 6 of the MAC) *(skipped if 0 switches)* |
| 6/7 | Green sticker check | Confirm with `y` | *(skipped if 0 switches)* |
| 7/7 | Dip switch check | Confirm the dip settings with `y` | See the on-screen diagram |

**Entering serial numbers:** you enter them one at a time. After the last one, the tester
lists them all and asks you to **confirm**:

- Press **`y`** to accept the list.
- Press **`n`** to re-enter the **last** one. (To fix an earlier one, use `b` to step back.)

The pair-serial screen shows a header like `Config 31: 3 pair(s), 1 switch(es)` — glance at it
to make sure the pair/switch counts are what you expect **before** the test starts.

---

## 5. Pre-test connectivity check

After setup, the tester **pings every pair** to make sure it can reach them.

- If all pairs answer, the test begins automatically.
- If a pair does **not** answer, you'll see:
  *"Pair X could not pass the preliminary test — Please check its connections."*
  Fix the cabling/power for that pair, then **press any key to try again**.

---

## 6. Running the test — the live screen

Once connectivity passes, the live monitor appears and the test runs continuously:

```
  LAN-EX H/F Tester                                         LIVE  press Q to stop & report

  Operator GS   Config 31   Cycle 4   Elapsed 00:14:32
------------------------------------------------------------------------------------------
  Phase A  max throughput - one pair at a time, 10s each direction
------------------------------------------------------------------------------------------
  Pair Serial              TX Mbps     RX Mbps   Status
  1    1234567-7654321         941         939   done
  2    2233445-5544332         938         942   done
  3    3344556-6655443         610           -   running
  4    4455667-7766554        drop           -   FAIL
  5    5566778-8877665           -           -   waiting

  Pair 3 TX [############--------] 62%  6s / 10s
------------------------------------------------------------------------------------------
  Totals  done 2  fail 1      target >= 90 / 190 Mbps
```

**Reading the screen:**

- **Top bar** — `LIVE` means the test is running. The reminder shows **press `Q` to stop**.
- **Info line** — your initials, the configuration, the current **cycle** number, and how long
  the run has been going (**Elapsed**).
- **Phase line** — whether you're in **Phase A** (throughput) or **Phase B** (soak).
- **Pair table** — one row per pair, updating live:
  - **TX Mbps / RX Mbps** — the measured speed. Green = met the target, red = below target.
  - **Status** — what the pair is doing right now (see colors below).
- **Progress bar** — the countdown for the current step (Phase A direction, or the Phase B soak).
- **Totals** — how many pairs have finished (`done`) or failed (`fail`) this cycle, plus the
  target speeds.

**Status colors and words:**

| Shows | Color | Meaning |
|-------|-------|---------|
| `waiting` | dim | not started yet |
| `running` | teal | being tested now |
| `retry 2/3` | amber | a connection couldn't start; the tester is retrying automatically |
| `done` | green | finished this cycle successfully |
| `FAIL` | red | this pair failed (missed its target speed, or its connection dropped) |

The `retry N/M` amber marker means a measurement didn't start and the tester is trying again
on its own — no action needed. If all retries fail, the pair is marked `FAIL` and the run
keeps going.

**Stopping the test:** press **`q`** (or `Q`) at any time. The test finishes what it's doing,
stops cleanly, and moves on to the results.

> Let the test run through **as many cycles as you need** for confidence. The longer it runs,
> the more chances each pair has to reveal an intermittent problem.

---

## 7. Understanding the result

Results are reported **per pair** — there is no single all-or-nothing verdict.

**A pair PASSES only if, in every cycle it ran, it:**

1. met **both** the TX and RX target speeds in Phase A, **and**
2. **never dropped** its connection (in Phase A or Phase B).

**A pair FAILS if, in any cycle, it:**

- came in **below target** on TX or RX, **or**
- **dropped its connection even once**. A single drop fails the pair.

When the test stops you'll see the **Run Summary**, for example:

```
Started: 2026-07-09 14:22:07
Ended:   2026-07-09 15:07:19
Cycles completed: 118

Pair 1 (1234567-7654321): PASS   peak TX 941 / RX 942 Mbps
Pair 2 (2233445-5544332): FAIL   peak TX 30 / RX 80 Mbps
Pair 3 (3344556-6655443): FAIL   peak TX 900 / RX 910 Mbps   (dropped)

1 / 3 pairs passed
```

`(dropped)` next to a pair means it failed because its connection dropped at least once.

Press any key to save the reports and exit.

---

## 8. Where the reports go

Reports are saved automatically when you stop. They live in the **`reports/`** folder:

- **Summary report** — `reports/<config>_<serial-or-switch>_<date_time>.txt`
  Contains your initials, the configuration, the **start → end time**, cycles completed, the
  per-pair PASS/FAIL breakdown, the "X / Y pairs passed" rollup, and a list of any
  **connection drops** and **errors** that occurred — so every failure is documented.
- **Engineering report** — `reports/eng/eng_<...>.txt`
  The detailed raw measurement logs, for troubleshooting.
- **`reports/allReports.txt`** — a running log with every run appended.

Each run gets its **own timestamped file**, so a new run never overwrites an old one.

---

## 9. Troubleshooting

| You see… | What it means | What to do |
|----------|---------------|------------|
| "Invalid initials / number / serial" | The entry didn't match the required format | Re-enter as prompted (the message says the expected format) |
| "Pair X could not pass the preliminary test" | That pair didn't answer the ping | Check its cable and power, then press a key to retry |
| A pair stuck on amber `retry N/M` | Its connection couldn't start; the tester is retrying | Wait — it resolves itself, or fails after the last retry |
| A pair goes red `FAIL` with `drop` | The connection dropped during the test | Note the pair; it's recorded in the report. Check cabling/hardware for that pair |
| A pair's TX or RX shown in red | The speed was below target | The pair fails on throughput; recorded in the report |
| The screen won't stop | You may not have pressed `q` on the test screen | Press `q` (or `Q`) while the live monitor is showing |

---

## 10. Quick reference — keys

| Key | Where | Does |
|-----|-------|------|
| `Enter` | Setup | Submit your answer |
| `b` | Setup | Go back one step |
| `y` / `n` | Confirmations | Accept / re-do |
| `q` or `Q` | Live test | Stop the test and save reports |
| any key | Result / error screens | Continue |

---

*Target speeds, phase durations, the soak cap, and the retry count are set by your site
administrator in `config/targetBandwidth.conf`. If the targets look wrong for your equipment,
raise it with them rather than changing it yourself.*
