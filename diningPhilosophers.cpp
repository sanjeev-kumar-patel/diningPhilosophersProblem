/*
 * Operating Systems (B.Tech 5th Sem) - Assignment #3
 * Problem : Dining-Philosophers Problem (Roll No. 22)
 * Language: C++ (Windows API threads + OS semaphores, works with old MinGW)
 *
 * Build : g++ -std=c++11 diningPhilosophers.cpp -o dining.exe
 * Run   : .\dining.exe <philosophers> <meals_each> <max_think_ms> <max_eat_ms> <mode>
 *         .\dining.exe            (no arguments -> interactive prompts)
 *
 * Modes (both are deadlock-free and use only counting semaphores):
 *   1 = Dijkstra/Tanenbaum solution: a philosopher eats only if BOTH
 *       neighbours are not eating. State array + one semaphore per philosopher.
 *   2 = Room (N-1) solution: at most N-1 philosophers may try to pick up
 *       forks at once, so a circular wait can never form.
 *
 * Output: timestamped trace of every state change, a live state row
 *         (T = Thinking, H = Hungry, E = Eating), and a final table of
 *         meals eaten, waiting times and total running time.
 */

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <cstdlib>
using namespace std;

/* ------------------------------------------------------------------
 * Counting semaphore: thin wrapper over the Windows kernel semaphore.
 *   wait()   -> P operation (decrement, block if count is 0)
 *   signal() -> V operation (increment, wake one waiter)
 * ------------------------------------------------------------------ */
class Semaphore {
    HANDLE h;

    Semaphore(const Semaphore&);              // not copyable
    Semaphore& operator=(const Semaphore&);

public:
    explicit Semaphore(int initial = 0) {
        h = CreateSemaphore(NULL, initial, 1000000, NULL);
    }
    ~Semaphore() { CloseHandle(h); }

    void wait()   { WaitForSingleObject(h, INFINITE); }
    void signal() { ReleaseSemaphore(h, 1, NULL); }
};

enum State { THINKING, HUNGRY, EATING };

/* Per-philosopher statistics */
struct Stats {
    int meals;
    double totalWaitMs;
    double maxWaitMs;
    Stats() : meals(0), totalWaitMs(0), maxWaitMs(0) {}
};

/* ------------------------------------------------------------------
 * Shared simulation data
 * ------------------------------------------------------------------ */
int N;                         // number of philosophers
int MEALS;                     // meals each philosopher must eat
int MAX_THINK_MS, MAX_EAT_MS;
int MODE;

LARGE_INTEGER freqQPC, startQPC;

// Mode 1 data (state[] is protected by mutexSem)
vector<State> state;
Semaphore mutexSem(1);
Semaphore* selfSem;            // one per philosopher, initial 0

// Mode 2 data
Semaphore* forkSem;            // one per fork, initial 1
Semaphore* roomSem;            // initial N-1

// Output / display (protected by logSem)
Semaphore logSem(1);
vector<char> shown;
vector<Stats> stats;

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */
int leftOf(int i)  { return (i + N - 1) % N; }
int rightOf(int i) { return (i + 1) % N; }

double nowMs() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - startQPC.QuadPart) * 1000.0 / (double)freqQPC.QuadPart;
}

string num(long long v) {
    ostringstream os;
    os << v;
    return os.str();
}

int randomMs(int maxMs, mt19937& rng) {
    return (int)(rng() % (unsigned)maxMs) + 1;
}

char stateChar(State s) {
    return s == THINKING ? 'T' : (s == HUNGRY ? 'H' : 'E');
}

/* Print one trace line plus the current state row of all philosophers */
void logEvent(int id, State s, const string& msg) {
    logSem.wait();
    shown[id] = stateChar(s);
    cout << "[" << setw(6) << (long long)nowMs() << " ms] Philosopher " << setw(2) << id
         << " " << left << setw(34) << msg << right << " | states: ";
    for (int k = 0; k < N; ++k) cout << shown[k] << ' ';
    cout << endl;
    logSem.signal();
}

/* ------------------------------------------------------------------
 * Mode 1: Dijkstra / Tanenbaum solution
 * ------------------------------------------------------------------ */
void test(int i) {
    // Philosopher i may eat only if hungry and neither neighbour is eating
    if (state[i] == HUNGRY && state[leftOf(i)] != EATING && state[rightOf(i)] != EATING) {
        state[i] = EATING;
        selfSem[i].signal();       // let philosopher i proceed
    }
}

void takeForksMode1(int i) {
    mutexSem.wait();
    state[i] = HUNGRY;
    logEvent(i, HUNGRY, "is hungry");
    test(i);
    mutexSem.signal();
    selfSem[i].wait();             // blocks here if forks are not available
}

void putForksMode1(int i) {
    mutexSem.wait();
    state[i] = THINKING;
    logEvent(i, THINKING, "put down forks, thinking");
    test(leftOf(i));               // a waiting neighbour may now eat
    test(rightOf(i));
    mutexSem.signal();
}

/* ------------------------------------------------------------------
 * Mode 2: at most N-1 philosophers compete for forks
 * ------------------------------------------------------------------ */
void takeForksMode2(int i) {
    logEvent(i, HUNGRY, "is hungry");
    roomSem->wait();               // enter the "room"
    forkSem[i].wait();             // left fork
    forkSem[rightOf(i)].wait();    // right fork
}

void putForksMode2(int i) {
    forkSem[i].signal();
    forkSem[rightOf(i)].signal();
    roomSem->signal();             // leave the "room"
    logEvent(i, THINKING, "put down forks, thinking");
}

/* ------------------------------------------------------------------
 * Philosopher thread
 * ------------------------------------------------------------------ */
DWORD WINAPI philosopher(LPVOID param) {
    int id = (int)(INT_PTR)param;
    mt19937 rng((unsigned)GetTickCount() ^ (unsigned)(id * 2654435761u));

    for (int meal = 1; meal <= MEALS; ++meal) {
        // THINK
        Sleep(randomMs(MAX_THINK_MS, rng));

        // HUNGRY -> acquire forks (measure waiting time)
        double hungrySince = nowMs();
        if (MODE == 1) takeForksMode1(id); else takeForksMode2(id);
        double waitMs = nowMs() - hungrySince;

        // EAT
        logEvent(id, EATING, "is EATING (meal " + num(meal) + ", waited " +
                                 num((long long)waitMs) + " ms)");
        Sleep(randomMs(MAX_EAT_MS, rng));

        stats[id].meals++;
        stats[id].totalWaitMs += waitMs;
        if (waitMs > stats[id].maxWaitMs) stats[id].maxWaitMs = waitMs;

        // PUT DOWN FORKS
        if (MODE == 1) putForksMode1(id); else putForksMode2(id);
    }
    logEvent(id, THINKING, "has finished all meals");
    return 0;
}

/* ------------------------------------------------------------------
 * Input handling
 * ------------------------------------------------------------------ */
void usage(const char* prog) {
    cout << "Usage: " << prog
         << " <philosophers> <meals_each> <max_think_ms> <max_eat_ms> <mode>\n"
         << "  philosophers : 2 to 20\n"
         << "  meals_each   : >= 1\n"
         << "  max_think_ms : >= 1\n"
         << "  max_eat_ms   : >= 1\n"
         << "  mode         : 1 = state/neighbour check (Tanenbaum), 2 = room (N-1)\n"
         << "Run with no arguments for interactive input.\n";
}

bool validInputs() {
    return N >= 2 && N <= 20 && MEALS >= 1 && MAX_THINK_MS >= 1 &&
           MAX_EAT_MS >= 1 && (MODE == 1 || MODE == 2);
}

int main(int argc, char* argv[]) {
    if (argc == 6) {
        N = atoi(argv[1]);
        MEALS = atoi(argv[2]);
        MAX_THINK_MS = atoi(argv[3]);
        MAX_EAT_MS = atoi(argv[4]);
        MODE = atoi(argv[5]);
    } else if (argc == 1) {
        cout << "Number of philosophers (2-20): ";       cin >> N;
        cout << "Meals each philosopher must eat: ";     cin >> MEALS;
        cout << "Max thinking time per round (ms): ";    cin >> MAX_THINK_MS;
        cout << "Max eating time per round (ms): ";      cin >> MAX_EAT_MS;
        cout << "Mode (1 = Tanenbaum, 2 = Room N-1): ";  cin >> MODE;
        if (!cin) { cout << "Invalid input.\n"; return 1; }
    } else {
        usage(argv[0]);
        return 1;
    }

    if (!validInputs()) {
        cout << "Invalid input values.\n";
        usage(argv[0]);
        return 1;
    }

    // Initialise shared data
    state.assign(N, THINKING);
    shown.assign(N, 'T');
    stats.assign(N, Stats());
    selfSem = new Semaphore[N];                         // all start at 0
    forkSem = new Semaphore[N];
    for (int i = 0; i < N; ++i) forkSem[i].signal();    // each fork starts at 1
    roomSem = new Semaphore(N - 1);

    cout << "\n=== Dining Philosophers: " << N << " philosophers, " << MEALS
         << " meals each, mode " << MODE
         << (MODE == 1 ? " (Tanenbaum)" : " (Room N-1)") << " ===\n"
         << "States: T = Thinking, H = Hungry, E = Eating\n\n";

    QueryPerformanceFrequency(&freqQPC);
    QueryPerformanceCounter(&startQPC);

    vector<HANDLE> threads(N);
    for (int i = 0; i < N; ++i)
        threads[i] = CreateThread(NULL, 0, philosopher, (LPVOID)(INT_PTR)i, 0, NULL);
    WaitForMultipleObjects(N, &threads[0], TRUE, INFINITE);
    for (int i = 0; i < N; ++i) CloseHandle(threads[i]);

    long long totalMs = (long long)nowMs();

    // Final report
    cout << "\n=== Summary ===\n";
    cout << left << setw(14) << "Philosopher" << setw(8) << "Meals"
         << setw(18) << "Avg wait (ms)" << setw(18) << "Max wait (ms)"
         << "Total wait (ms)\n";
    for (int i = 0; i < N; ++i) {
        double avg = stats[i].meals ? stats[i].totalWaitMs / stats[i].meals : 0;
        cout << left << setw(14) << i << setw(8) << stats[i].meals
             << setw(18) << fixed << setprecision(1) << avg
             << setw(18) << stats[i].maxWaitMs
             << stats[i].totalWaitMs << '\n';
    }
    cout << "\nAll philosophers finished. No deadlock, total time: " << totalMs << " ms\n";

    delete[] selfSem;
    delete[] forkSem;
    delete roomSem;
    return 0;
}