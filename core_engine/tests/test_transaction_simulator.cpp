#include "TransactionSimulator.hpp"

#include <cstdlib>
#include <iostream>
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

bool allCompleted(const std::vector<PCB>& processes) {
    for (const PCB& process : processes) {
        if (process.state != ProcessState::COMPLETED || process.remainingTime != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    const std::vector<Transaction> transactions{
        {501, TransactionType::DEPOSIT, 1, -1, 120.0, 2},
        {502, TransactionType::TRANSFER, 2, 3, 75.0, 5},
        {503, TransactionType::WITHDRAWAL, 4, -1, 30.0, 1}
    };

    TransactionSimulator simulator;
    simulator.loadTransactions(transactions);

    const std::vector<PCB>& initialProcesses = simulator.getProcesses();
    expect(initialProcesses.size() == transactions.size(),
           "Simulator creates one PCB for each transaction");
    bool transactionIDsMatch = initialProcesses.size() == transactions.size();
    bool prioritiesMatch = initialProcesses.size() == transactions.size();
    bool allStartNew = initialProcesses.size() == transactions.size();
    for (std::size_t index = 0; index < initialProcesses.size() && index < transactions.size(); ++index) {
        transactionIDsMatch = transactionIDsMatch &&
                              initialProcesses[index].transactionID == transactions[index].transactionID;
        prioritiesMatch = prioritiesMatch &&
                          initialProcesses[index].priority == transactions[index].priority;
        allStartNew = allStartNew && initialProcesses[index].state == ProcessState::NEW;
    }
    expect(transactionIDsMatch, "Generated PCBs preserve transaction IDs");
    expect(prioritiesMatch, "Generated PCBs preserve transaction priorities");
    expect(allStartNew, "Generated PCBs start in NEW state");

    const auto fcfsResults = simulator.runScheduling(SchedulingAlgorithm::FCFS);
    expect(fcfsResults.size() == transactions.size() && allCompleted(simulator.getProcesses()),
           "FCFS schedules the generated PCBs");

    const auto roundRobinResults = simulator.runScheduling(SchedulingAlgorithm::ROUND_ROBIN, 2);
    expect(roundRobinResults.size() == transactions.size() && allCompleted(simulator.getProcesses()),
           "Round Robin schedules the generated PCBs with the configured quantum");

    const auto priorityResults = simulator.runScheduling(SchedulingAlgorithm::PRIORITY);
    expect(priorityResults.size() == transactions.size() && allCompleted(simulator.getProcesses()),
           "Priority scheduling schedules the generated PCBs");

    {
        TransactionSimulator executionSimulator;
        executionSimulator.loadTransactions({
            {601, TransactionType::DEPOSIT, 1, -1, 20.0, 3}
        }, 0, 1);
        PCB& process = executionSimulator.getProcesses()[0];
        const Resource* resource = executionSimulator.getResourceManager().getResource(
            TransactionSimulator::SIMULATED_RESOURCE_ID);
        expect(resource != nullptr && resource->name == "CPU_IO" && resource->availableUnits == 1,
               "Simulator registers one available CPU_IO unit");
        expect(executionSimulator.executeProcess(process),
               "Process executes while CPU_IO is available");
        expect(process.state == ProcessState::COMPLETED && process.remainingTime == 0,
               "One-unit process completes after execution");
        resource = executionSimulator.getResourceManager().getResource(
            TransactionSimulator::SIMULATED_RESOURCE_ID);
        expect(resource != nullptr && resource->availableUnits == 1 &&
               executionSimulator.getResourceManager().getAllocatedUnits(
                   process, TransactionSimulator::SIMULATED_RESOURCE_ID) == 0,
               "CPU_IO is released after successful execution");
    }

    {
        TransactionSimulator blockedSimulator;
        blockedSimulator.loadTransactions({
            {602, TransactionType::TRANSFER, 2, 3, 45.0, 4}
        }, 0, 3);
        PCB& process = blockedSimulator.getProcesses()[0];
        Transaction holderTransaction{999, TransactionType::DEPOSIT, 8, -1, 1.0, 1};
        PCB holder = PCB::createFromTransaction(holderTransaction);
        ResourceManager& resources = blockedSimulator.getResourceManager();
        const bool occupied = resources.allocate(
            holder, TransactionSimulator::SIMULATED_RESOURCE_ID, 1);
        expect(occupied, "Test process can occupy the CPU_IO resource");
        expect(!blockedSimulator.executeProcess(process),
               "Execution fails gracefully when CPU_IO is unavailable");
        expect(process.state == ProcessState::WAITING && process.remainingTime == 3,
               "Blocked process waits without losing burst time");
        expect(resources.getResource(TransactionSimulator::SIMULATED_RESOURCE_ID)->availableUnits == 0,
               "Unavailable CPU_IO remains allocated to its owner");
        expect(resources.release(holder, TransactionSimulator::SIMULATED_RESOURCE_ID, 1),
               "CPU_IO can be released by its owning process");
    }

    {
        TransactionSimulator partialSimulator;
        partialSimulator.loadTransactions({
            {603, TransactionType::WITHDRAWAL, 4, -1, 15.0, 1}
        }, 0, 2);
        PCB& process = partialSimulator.getProcesses()[0];
        expect(partialSimulator.executeProcess(process) && process.remainingTime == 1 &&
               process.state == ProcessState::READY,
               "Execution performs one unit of work and readies an unfinished process");
        expect(partialSimulator.getResourceManager().getResource(
                   TransactionSimulator::SIMULATED_RESOURCE_ID)->availableUnits == 1,
               "CPU_IO is released after partial execution");
    }

    {
        TransactionSimulator synchronizedSimulator;
        synchronizedSimulator.loadTransactions({
            {701, TransactionType::DEPOSIT, 1, -1, 10.0, 1},
            {702, TransactionType::TRANSFER, 2, 3, 12.0, 2}
        }, 0, 2);
        std::vector<PCB>& processes = synchronizedSimulator.getProcesses();
        SynchronizationManager& synchronization = synchronizedSimulator.getSynchronizationManager();
        const Mutex* criticalSection = synchronization.getMutex(
            TransactionSimulator::TRANSACTION_MUTEX_NAME);
        expect(criticalSection != nullptr && !criticalSection->isLocked(),
               "Simulator registers the unlocked shared transaction mutex");

        expect(synchronization.acquireMutex(processes[0],
                                            TransactionSimulator::TRANSACTION_MUTEX_NAME),
               "First process acquires the transaction critical section");
        criticalSection = synchronization.getMutex(TransactionSimulator::TRANSACTION_MUTEX_NAME);
        expect(criticalSection != nullptr && criticalSection->isLocked() &&
               criticalSection->getOwnerProcessID() == processes[0].processID,
               "Shared mutex reports its owner");

        const int blockedRemainingTime = processes[1].remainingTime;
        expect(!synchronizedSimulator.executeWithSynchronization(processes[1].processID),
               "Second process cannot enter an owned critical section");
        expect(processes[1].state == ProcessState::WAITING &&
               processes[1].remainingTime == blockedRemainingTime,
               "Blocked process waits without using burst time");

        expect(synchronization.releaseMutex(processes[0],
                                            TransactionSimulator::TRANSACTION_MUTEX_NAME),
               "First process releases the transaction critical section");
        expect(synchronizedSimulator.executeWithSynchronization(processes[1].processID),
               "Second process executes after the mutex becomes available");
        expect(processes[1].state == ProcessState::READY && processes[1].remainingTime == 1,
               "Unfinished synchronized process returns to READY after one unit");
        criticalSection = synchronization.getMutex(TransactionSimulator::TRANSACTION_MUTEX_NAME);
        expect(criticalSection != nullptr && !criticalSection->isLocked(),
               "Successful synchronized execution releases the mutex");

        expect(synchronizedSimulator.executeWithSynchronization(processes[0].processID),
               "First process can execute after releasing the mutex");
        expect(processes[0].state == ProcessState::READY && processes[0].remainingTime == 1,
               "First unfinished process returns to READY");
        expect(!synchronizedSimulator.executeWithSynchronization(-1),
               "Unknown process ID is rejected");
    }

    {
        TransactionSimulator oneUnitSimulator;
        oneUnitSimulator.loadTransactions({
            {703, TransactionType::WITHDRAWAL, 4, -1, 5.0, 1}
        }, 0, 1);
        PCB& process = oneUnitSimulator.getProcesses()[0];
        expect(oneUnitSimulator.executeWithSynchronization(process.processID) &&
               process.state == ProcessState::COMPLETED && process.remainingTime == 0,
               "One-unit synchronized process reaches COMPLETED");
        const Mutex* criticalSection = oneUnitSimulator.getSynchronizationManager().getMutex(
            TransactionSimulator::TRANSACTION_MUTEX_NAME);
        expect(criticalSection != nullptr && !criticalSection->isLocked(),
               "Mutex is released after one-unit process completion");
    }

    {
        const std::vector<Transaction> endToEndTransactions{
            {801, TransactionType::DEPOSIT, 1, -1, 50.0, 2},
            {802, TransactionType::TRANSFER, 2, 3, 25.0, 5},
            {803, TransactionType::LOAN_PAYMENT, 4, -1, 10.0, 1}
        };
        TransactionSimulator endToEndSimulator;
        expect(endToEndSimulator.runEndToEndSimulation(endToEndTransactions),
               "End-to-end simulation completes all supplied transactions");

        const SimulationSummary firstSummary = endToEndSimulator.getSimulationSummary();
        expect(firstSummary.totalTransactions == 3,
               "Simulation summary reports the total transaction count");
        expect(firstSummary.completedTransactions == 3,
               "Simulation summary reports all transactions completed");
        expect(firstSummary.waitingTransactions == 0 && firstSummary.failedTransactions == 0,
               "Successful simulation has no waiting or failed transactions");

        const std::vector<PCB> firstRunProcesses = endToEndSimulator.getProcesses();
        bool recordsPreserved = firstRunProcesses.size() == endToEndTransactions.size();
        bool recordsCompleted = firstRunProcesses.size() == endToEndTransactions.size();
        for (std::size_t index = 0; index < firstRunProcesses.size() &&
                                    index < endToEndTransactions.size(); ++index) {
            recordsPreserved = recordsPreserved &&
                               firstRunProcesses[index].transactionID ==
                                   endToEndTransactions[index].transactionID &&
                               firstRunProcesses[index].priority == endToEndTransactions[index].priority;
            recordsCompleted = recordsCompleted &&
                               firstRunProcesses[index].state == ProcessState::COMPLETED &&
                               firstRunProcesses[index].remainingTime == 0;
        }
        expect(recordsPreserved, "End-to-end PCBs preserve transaction IDs and priorities");
        expect(recordsCompleted, "Every end-to-end PCB completes with zero remaining time");

        expect(endToEndSimulator.runEndToEndSimulation(endToEndTransactions),
               "Repeated end-to-end simulation also completes successfully");
        const std::vector<PCB>& secondRunProcesses = endToEndSimulator.getProcesses();
        bool sameTransactionOutcomes = secondRunProcesses.size() == firstRunProcesses.size();
        for (std::size_t index = 0; index < secondRunProcesses.size() &&
                                    index < firstRunProcesses.size(); ++index) {
            sameTransactionOutcomes = sameTransactionOutcomes &&
                                      secondRunProcesses[index].transactionID ==
                                          firstRunProcesses[index].transactionID &&
                                      secondRunProcesses[index].priority == firstRunProcesses[index].priority &&
                                      secondRunProcesses[index].state == firstRunProcesses[index].state &&
                                      secondRunProcesses[index].remainingTime ==
                                          firstRunProcesses[index].remainingTime;
        }
        const SimulationSummary secondSummary = endToEndSimulator.getSimulationSummary();
        expect(sameTransactionOutcomes &&
               secondSummary.completedTransactions == firstSummary.completedTransactions &&
               secondSummary.waitingTransactions == firstSummary.waitingTransactions &&
               secondSummary.failedTransactions == firstSummary.failedTransactions,
               "Repeated run produces the same transaction outcomes and summary");
    }

    std::cout << "\nTransaction simulator tests: " << testsPassed << " passed, "
              << testsFailed << " failed.\n";
    return testsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
