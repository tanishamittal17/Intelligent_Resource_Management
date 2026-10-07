#include "Scheduler.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int testsPassed = 0;
int testsFailed = 0;

void expect(bool condition, const std::string& description) {
    if (condition) {
        ++testsPassed;
        std::cout << "[PASS] " << description << '\n';
    } else {
        ++testsFailed;
        std::cout << "[FAIL] " << description << '\n';
    }
}

PCB makeProcess(int processID, int arrivalTime, int burstTime, int priority = 0) {
    Transaction transaction{processID * 10, TransactionType::TRANSFER, 1, 2, 10.0, priority};
    return PCB(processID, transaction, arrivalTime, burstTime);
}

} // namespace

int main() {
    const Transaction fetchedTransaction{909, TransactionType::DEPOSIT, 4, -1, 25.0, 6};
    const PCB transactionProcess = PCB::createFromTransaction(fetchedTransaction);
    {
        std::vector<PCB> processes{transactionProcess};
        const auto results = FCFSScheduler{}.schedule(processes);
        expect(results.size() == 1 && results[0].processID == transactionProcess.processID &&
               processes[0].state == ProcessState::COMPLETED,
               "FCFS accepts a PCB created from a transaction");
    }
    {
        std::vector<PCB> processes{transactionProcess};
        const auto results = RoundRobinScheduler(1).schedule(processes);
        expect(results.size() == 1 && results[0].processID == transactionProcess.processID &&
               processes[0].state == ProcessState::COMPLETED,
               "Round Robin accepts a PCB created from a transaction");
    }
    {
        std::vector<PCB> processes{transactionProcess};
        const auto results = PriorityScheduler{}.schedule(processes);
        expect(results.size() == 1 && results[0].processID == transactionProcess.processID &&
               processes[0].state == ProcessState::COMPLETED,
               "Priority scheduling accepts a PCB created from a transaction");
    }

    {
        // Input order differs from arrival order. P2 arrives at time 1, P1 at time 2.
        std::vector<PCB> processes{
            makeProcess(1, 2, 3),
            makeProcess(2, 1, 2),
            makeProcess(3, 4, 1)
        };
        const auto results = FCFSScheduler{}.schedule(processes);
        expect(results[1].completionTime == 3 && results[1].waitingTime == 0 &&
               results[1].turnaroundTime == 2, "FCFS starts with earliest arrival");
        expect(results[0].completionTime == 6 && results[0].waitingTime == 1 &&
               results[0].turnaroundTime == 4, "FCFS computes P1 metrics");
        expect(results[2].completionTime == 7 && results[2].waitingTime == 2 &&
               results[2].turnaroundTime == 3, "FCFS computes P3 metrics");
        expect(processes[0].state == ProcessState::COMPLETED && processes[0].remainingTime == 0,
               "FCFS marks finished process completed");
    }

    {
        std::vector<PCB> processes{
            makeProcess(1, 0, 5),
            makeProcess(2, 1, 3),
            makeProcess(3, 2, 1)
        };
        const auto results = RoundRobinScheduler(2).schedule(processes);
        expect(results[0].completionTime == 9 && results[0].waitingTime == 4 &&
               results[0].turnaroundTime == 9, "Round Robin computes P1 metrics");
        expect(results[1].completionTime == 8 && results[1].waitingTime == 4 &&
               results[1].turnaroundTime == 7, "Round Robin computes P2 metrics");
        expect(results[2].completionTime == 5 && results[2].waitingTime == 2 &&
               results[2].turnaroundTime == 3, "Round Robin admits arrivals during a slice");
        expect(processes[0].state == ProcessState::COMPLETED &&
               processes[1].state == ProcessState::COMPLETED &&
               processes[2].state == ProcessState::COMPLETED, "Round Robin completes all PCBs");
    }

    {
        std::vector<PCB> processes{
            makeProcess(1, 0, 4, 1),
            makeProcess(2, 0, 2, 5),
            makeProcess(3, 1, 1, 3)
        };
        const auto results = PriorityScheduler{}.schedule(processes);
        expect(results[1].completionTime == 2 && results[1].waitingTime == 0 &&
               results[1].turnaroundTime == 2, "Priority runs larger priority value first");
        expect(results[2].completionTime == 3 && results[2].waitingTime == 1 &&
               results[2].turnaroundTime == 2, "Priority selects highest arrived process next");
        expect(results[0].completionTime == 7 && results[0].waitingTime == 3 &&
               results[0].turnaroundTime == 7, "Priority computes remaining process metrics");
    }

    {
        bool rejected = false;
        try {
            (void)RoundRobinScheduler(0);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected, "Round Robin rejects a non-positive quantum");
    }

    std::cout << "\nScheduler tests: " << testsPassed << " passed, " << testsFailed << " failed.\n";
    return testsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
