#include "TransactionSimulator.hpp"

#include <stdexcept>

void TransactionSimulator::loadTransactions(const std::vector<Transaction>& transactions,
                                             int arrivalTime,
                                             int burstTime) {
    processes.clear();
    processes.reserve(transactions.size());
    for (const Transaction& transaction : transactions) {
        processes.push_back(PCB::createFromTransaction(transaction, arrivalTime, burstTime));
    }
}

const std::vector<PCB>& TransactionSimulator::getProcesses() const noexcept {
    return processes;
}

std::vector<SchedulingResult> TransactionSimulator::runScheduling(SchedulingAlgorithm algorithm,
                                                                  int timeQuantum) {
    switch (algorithm) {
        case SchedulingAlgorithm::FCFS:
            return FCFSScheduler{}.schedule(processes);
        case SchedulingAlgorithm::ROUND_ROBIN:
            return RoundRobinScheduler(timeQuantum).schedule(processes);
        case SchedulingAlgorithm::PRIORITY:
            return PriorityScheduler{}.schedule(processes);
    }
    throw std::invalid_argument("Unknown scheduling algorithm");
}
