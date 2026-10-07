#pragma once

#include "Scheduler.hpp"

#include <vector>

enum class SchedulingAlgorithm {
    FCFS,
    ROUND_ROBIN,
    PRIORITY
};

class TransactionSimulator {
public:
    // Each loaded transaction becomes a PCB with these simple timing defaults.
    void loadTransactions(const std::vector<Transaction>& transactions,
                          int arrivalTime = 0,
                          int burstTime = 5);

    const std::vector<PCB>& getProcesses() const noexcept;

    // Schedules the stored PCBs using the existing scheduler implementations.
    // timeQuantum is used only by Round Robin.
    std::vector<SchedulingResult> runScheduling(SchedulingAlgorithm algorithm,
                                                int timeQuantum = 1);

private:
    std::vector<PCB> processes;
};
