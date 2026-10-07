#pragma once

#include "ResourceManager.hpp"
#include "Scheduler.hpp"
#include "SynchronizationManager.hpp"

#include <vector>

enum class SchedulingAlgorithm {
    FCFS,
    ROUND_ROBIN,
    PRIORITY
};

class TransactionSimulator {
public:
    static constexpr int SIMULATED_RESOURCE_ID = 1;
    static constexpr int TRANSACTION_MUTEX_ID = 1;
    inline static constexpr char TRANSACTION_MUTEX_NAME[] = "TRANSACTION_CRITICAL_SECTION";

    TransactionSimulator();

    // Each loaded transaction becomes a PCB with these simple timing defaults.
    void loadTransactions(const std::vector<Transaction>& transactions,
                          int arrivalTime = 0,
                          int burstTime = 5);

    const std::vector<PCB>& getProcesses() const noexcept;
    std::vector<PCB>& getProcesses() noexcept;

    ResourceManager& getResourceManager() noexcept;
    const ResourceManager& getResourceManager() const noexcept;
    SynchronizationManager& getSynchronizationManager() noexcept;
    const SynchronizationManager& getSynchronizationManager() const noexcept;

    // Schedules the stored PCBs using the existing scheduler implementations.
    // timeQuantum is used only by Round Robin.
    std::vector<SchedulingResult> runScheduling(SchedulingAlgorithm algorithm,
                                                int timeQuantum = 1);

    // Simulates one unit of CPU work while holding the shared CPU_IO resource.
    bool executeProcess(PCB& process);

    // Runs one unit while owning the shared transaction critical-section mutex.
    bool executeWithSynchronization(int processID);

private:
    std::vector<PCB> processes;
    ResourceManager resourceManager;
    SynchronizationManager synchronizationManager;
};
