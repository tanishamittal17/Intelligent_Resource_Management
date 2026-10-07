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

    std::cout << "\nTransaction simulator tests: " << testsPassed << " passed, "
              << testsFailed << " failed.\n";
    return testsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
