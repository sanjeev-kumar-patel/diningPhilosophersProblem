# Dining-Philosophers Problem

**Course:** Operating Systems (B.Tech 5th Semester), Assignment #3
**Roll No.:** 22  |  **Topic:** Dining-Philosophers Problem
**Language:** C++ (Windows API threads and kernel semaphores)
**Source file:** `diningPhilosophers.cpp`

---

## 1. Problem Formulation

N philosophers sit around a circular table. Between each pair of neighbours there is one fork, so there are N forks. Each philosopher repeats the cycle **think -> hungry -> eat**. To eat, a philosopher needs **both** the left and the right fork. A fork can be used by only one philosopher at a time.

The program must avoid two problems:

| Problem | Meaning |
|---|---|
| **Deadlock** | Every philosopher holds one fork and waits for the other, so nobody can ever eat. |
| **Starvation** | The system keeps running, but one philosopher waits indefinitely. |

Neighbour formulas used (indices wrap around the circle):
- `left(i)  = (i + N - 1) % N`
- `right(i) = (i + 1) % N`

---

## 2. Algorithms Implemented

Both solutions use only **counting/binary semaphores**.

### Mode 1: Dijkstra / Tanenbaum solution
A philosopher eats only if they are hungry **and neither neighbour is eating**.

Data: `state[i]` (THINKING / HUNGRY / EATING), `mutexSem = 1` (guards `state[]`), `selfSem[i] = 0` (one per philosopher).

```
test(i):
    if state[i] == HUNGRY and state[left] != EATING and state[right] != EATING:
        state[i] = EATING
        signal(selfSem[i])

take_forks(i):
    wait(mutexSem)
    state[i] = HUNGRY
    test(i)
    signal(mutexSem)
    wait(selfSem[i])        // blocks if forks are not available

put_forks(i):
    wait(mutexSem)
    state[i] = THINKING
    test(left); test(right) // wake a hungry neighbour if it can now eat
    signal(mutexSem)
```
**Why no deadlock:** a philosopher gets both forks together or none, so the *hold-and-wait* condition never occurs.
`wait(selfSem[i])` is outside the mutex so a sleeping philosopher never blocks the neighbour who must wake them.

### Mode 2: Room (N-1) solution
Data: `forkSem[i] = 1` for each fork, `roomSem = N-1`.

```
take_forks(i):  wait(room); wait(fork[i]); wait(fork[right(i)])
put_forks(i):   signal(fork[i]); signal(fork[right(i)]); signal(room)
```
**Why no deadlock:** at most N-1 philosophers compete for forks at once, so by the pigeonhole principle at least one can always obtain both forks. This breaks the *circular wait* condition.

---

## 3. Build and Run

```
g++ -std=c++11 diningPhilosophers.cpp -o dining.exe
.\dining.exe <philosophers> <meals_each> <max_think_ms> <max_eat_ms> <mode>
```

| Argument | Meaning | Valid range |
|---|---|---|
| philosophers | number of philosophers (N) | 2 to 20 |
| meals_each | meals every philosopher must eat | >= 1 |
| max_think_ms | maximum random thinking time | >= 1 |
| max_eat_ms | maximum random eating time | >= 1 |
| mode | 1 = Tanenbaum, 2 = Room (N-1) | 1 or 2 |

Running `.\dining.exe` with **no arguments** asks for each value interactively. No test values are hardcoded.

Example: `.\dining.exe 5 3 200 200 1`

---

## 4. Sample Input and Output

**Input:** `.\dining.exe 5 2 100 100 1`  (5 philosophers, 2 meals each, think/eat up to 100 ms, Mode 1)

States: **T** = Thinking, **H** = Hungry, **E** = Eating. The row on the right shows all philosophers after each event.

```
=== Dining Philosophers: 5 philosophers, 2 meals each, mode 1 (Tanenbaum) ===
States: T = Thinking, H = Hungry, E = Eating

[    23 ms] Philosopher  2 is hungry                        | states: T T H T T
[    23 ms] Philosopher  2 is EATING (meal 1, waited 0 ms)  | states: T T E T T
[    30 ms] Philosopher  4 is hungry                        | states: T T E T H
[    30 ms] Philosopher  4 is EATING (meal 1, waited 0 ms)  | states: T T E T E
[    50 ms] Philosopher  4 put down forks, thinking         | states: T T E T T
[    53 ms] Philosopher  1 is hungry                        | states: T H E T T
[    60 ms] Philosopher  0 is hungry                        | states: H H E T T
[    60 ms] Philosopher  0 is EATING (meal 1, waited 0 ms)  | states: E H E T T
...
[   285 ms] Philosopher  0 has finished all meals           | states: T T T T T

=== Summary ===
Philosopher   Meals   Avg wait (ms)     Max wait (ms)     Total wait (ms)
0             2       15.3              30.6              30.6
1             2       19.2              38.5              38.5
2             2       46.3              92.5              92.6
3             2       28.0              38.8              56.0
4             2       7.0               14.0              14.0

All philosophers finished. No deadlock, total time: 285 ms
```

**Reading the trace:** philosophers 2 and 4 eat together because they are not neighbours. Philosopher 1 becomes hungry while philosopher 2 (a neighbour) is eating, so philosopher 1 must wait. Philosopher 1 only eats at 91 ms, after philosopher 2 puts down the forks. The row never shows two adjacent `E` letters, which shows mutual exclusion of forks holds.

Times differ on every run because thinking and eating times are random. Mode 2 is run the same way with `... 2` as the last argument.

---

## 5. Line-by-Line Explanation of the Code

### 5.1 Headers and setup
- `#include <windows.h>` provides the Windows API for threads, kernel semaphores, `Sleep` and the high-resolution timer.
- `<iostream> <iomanip> <vector> <string> <sstream> <random> <cstdlib>` cover printing, formatted tables, arrays, string building, random numbers and `atoi`.
- `using namespace std;` lets the code write `cout` instead of `std::cout`.
- The Windows API is used instead of `std::thread` because the lab compiler (MinGW GCC 6.3, win32 thread model) does not include `std::thread`, `std::mutex` or `std::condition_variable`.

### 5.2 `Semaphore` class
| Line | Explanation |
|---|---|
| `HANDLE h;` | Handle to a kernel semaphore object owned by the OS. |
| Private copy constructor and `operator=` (declared, never defined) | Prevents copying, since copying a handle would break the count. |
| `explicit Semaphore(int initial = 0)` | Calls `CreateSemaphore(NULL, initial, 1000000, NULL)`: default security, initial count, maximum count, no name. |
| `~Semaphore()` | `CloseHandle(h)` releases the OS object. |
| `wait()` | **P operation**: `WaitForSingleObject(h, INFINITE)` decrements the count, or blocks if the count is 0. |
| `signal()` | **V operation**: `ReleaseSemaphore(h, 1, NULL)` increments the count and wakes one waiter. |

A semaphore of value 1 acts as a mutex. A semaphore starting at 0 acts as a "wait until allowed" signal.

### 5.3 Data definitions
- `enum State { THINKING, HUNGRY, EATING }` gives the three philosopher states.
- `struct Stats` holds `meals`, `totalWaitMs` and `maxWaitMs` for each philosopher, all initialised to 0.

| Global | Purpose |
|---|---|
| `N, MEALS, MAX_THINK_MS, MAX_EAT_MS, MODE` | Test configuration from user input |
| `freqQPC, startQPC` | High-resolution timer values |
| `state` | State of each philosopher (Mode 1) |
| `mutexSem` | Binary semaphore (1) guarding `state[]` |
| `selfSem[]` | One semaphore per philosopher, initial 0 |
| `forkSem[]` | One semaphore per fork, initial 1 (Mode 2) |
| `roomSem` | Initial N-1 (Mode 2) |
| `logSem` | Binary semaphore so output lines do not mix |
| `shown` | The T/H/E letters printed in the state row |
| `stats` | Per-philosopher statistics |

### 5.4 Helper functions
- `leftOf(i)`, `rightOf(i)` give the circular neighbours. `% N` wraps around, and `+ N` keeps the result non-negative.
- `nowMs()` returns elapsed milliseconds using `QueryPerformanceCounter`, which is more accurate than `GetTickCount`.
- `num(v)` converts a number to a string with `ostringstream` (`std::to_string` is unreliable on old MinGW).
- `randomMs(maxMs, rng)` returns a random time from 1 to `maxMs`.
- `stateChar(s)` converts a state to `T`, `H` or `E`.
- `logEvent(id, s, msg)` locks `logSem`, updates `shown[id]`, prints the timestamp, philosopher number, message and the full state row, then unlocks `logSem`.

### 5.5 Mode 1 functions
- `test(i)`: if philosopher `i` is HUNGRY and neither neighbour is EATING, set `state[i] = EATING` and `signal(selfSem[i])` to wake them.
- `takeForksMode1(i)`: lock `mutexSem`, set HUNGRY, log, call `test(i)`, unlock `mutexSem`, then `selfSem[i].wait()`. If `test` succeeded the count is already 1 and the wait passes straight through. Otherwise the philosopher sleeps until a neighbour finishes.
- `putForksMode1(i)`: lock `mutexSem`, set THINKING, log, call `test` on the left and right neighbours so a hungry one can start eating, unlock `mutexSem`.

### 5.6 Mode 2 functions
- `takeForksMode2(i)`: log hungry, `roomSem->wait()` (enter the room, at most N-1 people), then wait for the left fork and the right fork.
- `putForksMode2(i)`: signal both forks, `roomSem->signal()` (leave the room), then log.

### 5.7 Philosopher thread
```cpp
DWORD WINAPI philosopher(LPVOID param)
```
- `id = (int)(INT_PTR)param` recovers the philosopher number, because the Windows API only passes a pointer.
- Each thread creates its own `mt19937` random generator with a different seed.
- For each meal the loop does: `Sleep` (think), record `hungrySince`, acquire forks, compute `waitMs = nowMs() - hungrySince`, log EATING, `Sleep` (eat), update `stats`, release forks.
- After the last meal it logs "has finished all meals".

### 5.8 Input handling and `main`
- `usage()` prints the correct command format.
- `validInputs()` rejects N outside 2-20, non-positive values and a mode other than 1 or 2.
- `main()` reads 5 command-line arguments, or prompts interactively if there are none, then validates the input.
- It allocates `selfSem` (all 0), `forkSem` (each signalled once so it starts at 1) and `roomSem` (N-1).
- It starts the timer, creates N threads with `CreateThread`, then `WaitForMultipleObjects(N, ..., TRUE, INFINITE)` waits for all of them (like `join`).
- It prints the summary table (meals, average/max/total wait) and the total run time, then frees the memory.

---

## 6. Viva Questions and Answers

**What are the four conditions for deadlock?**
Mutual exclusion, hold and wait, no preemption, circular wait. Mode 1 breaks hold-and-wait (both forks together or none). Mode 2 breaks circular wait (at most N-1 competitors).

**Deadlock vs starvation?**
In deadlock nobody can ever progress. In starvation the system progresses, but one process may wait indefinitely. Mode 1 is deadlock-free but could in theory starve a philosopher whose two neighbours keep taking turns. The max-wait column in the summary table helps to observe this.

**What is the critical section here?**
Reading and writing `state[]` (guarded by `mutexSem`) and printing (guarded by `logSem`).

**What race condition could occur without `mutexSem`?**
Two neighbours could run `test()` at the same time, both see the other as not eating, and both start eating, which means two philosophers sharing a fork.

**Binary vs counting semaphore?**
Binary semaphores (`mutexSem`, `logSem`, forks) are 0 or 1. Counting semaphores (`roomSem` = N-1) hold larger values.

**Why is `selfSem[i].wait()` outside the mutex?**
Sleeping while holding the mutex would block everyone, including the neighbour who must wake the sleeper, which would cause deadlock.

**Which system calls are used?**
`CreateThread`, `CreateSemaphore`, `WaitForSingleObject`, `ReleaseSemaphore`, `WaitForMultipleObjects`, `CloseHandle`, `Sleep`. They are the Windows equivalents of `pthread_create`, `sem_init`, `sem_wait`, `sem_post` and `pthread_join`.

**Can the problem be solved without semaphores?**
Yes: with a monitor and condition variables, or with resource ordering (always pick up the lower-numbered fork first).

---

## 7. Files

| File | Description |
|---|---|
| `diningPhilosophers.cpp` | Source code |
| `README.md` | This document |