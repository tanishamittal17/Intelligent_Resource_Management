#include "TransactionSimulator.hpp"

#include <algorithm>
#include <stdexcept>

TransactionSimulator::TransactionSimulator() {
    resourceManager.addResource(Resource{SIMULATED_RESOURCE_ID, "CPU_IO", 1});
    if (synchronizationManager.getMutex(TRANSACTION_MUTEX_NAME) == nullptr) {
        synchronizationManager.addMutex(Mutex{TRANSACTION_MUTEX_ID, TRANSACTION_MUTEX_NAME});
    }
}

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

std::vector<PCB>& TransactionSimulator::getProcesses() noexcept {
    return processes;
}

ResourceManager& TransactionSimulator::getResourceManager() noexcept {
    return resourceManager;
}

const ResourceManager& TransactionSimulator::getResourceManager() const noexcept {
    return resourceManager;
}

SynchronizationManager& TransactionSimulator::getSynchronizationManager() noexcept {
    return synchronizationManager;
}

const SynchronizationManager& TransactionSimulator::getSynchronizationManager() const noexcept {
    return synchronizationManager;
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

bool TransactionSimulator::executeProcess(PCB& process) {
    if (process.state == ProcessState::COMPLETED || process.state == ProcessState::FAILED ||
        process.remainingTime <= 0) {
        return false;
    }

    process.changeState(ProcessState::RUNNING);
    if (!resourceManager.allocate(process, SIMULATED_RESOURCE_ID, 1)) {
        process.changeState(ProcessState::WAITING);
        return false;
    }

    process.updateRemainingTime(1);
    const bool resourceReleased = resourceManager.release(process, SIMULATED_RESOURCE_ID, 1);
    if (!resourceReleased) {
        process.changeState(ProcessState::FAILED);
        return false;
    }

    if (process.remainingTime == 0) {
        process.changeState(ProcessState::COMPLETED);
    } else {
        process.changeState(ProcessState::READY);
    }
    return true;
}

bool TransactionSimulator::executeWithSynchronization(int processID) {
    auto process = std::find_if(processes.begin(), processes.end(), [processID](const PCB& candidate) {
        return candidate.processID == processID;
    });
    if (process == processes.end() || process->state == ProcessState::COMPLETED ||
        process->state == ProcessState::FAILED || process->remainingTime <= 0) {
        return false;
    }

    if (!synchronizationManager.acquireMutex(*process, TRANSACTION_MUTEX_NAME)) {
        process->changeState(ProcessState::WAITING);
        return false;
    }

    process->changeState(ProcessState::RUNNING);
    process->updateRemainingTime(1);
    const bool completed = process->remainingTime == 0;
    const bool released = synchronizationManager.releaseMutex(*process, TRANSACTION_MUTEX_NAME);
    if (!released) {
        process->changeState(ProcessState::FAILED);
        return false;
    }

    process->changeState(completed ? ProcessState::COMPLETED : ProcessState::READY);
    return true;
}
