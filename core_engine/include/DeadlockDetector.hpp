#pragma once

#include "PCB.hpp"

#include <map>
#include <set>
#include <vector>

enum class DeadlockStatus {
    NO_DEADLOCK,
    DEADLOCK_DETECTED
};

struct DeadlockResult {
    DeadlockStatus status;
    std::vector<int> processIDs;

    bool hasDeadlock() const { return status == DeadlockStatus::DEADLOCK_DETECTED; }
};

class DeadlockDetector {
public:
    bool addProcess(int processID);
    bool addProcess(const PCB& process);
    bool removeProcess(int processID);

    // Adds an edge from the waiting process to the process holding its resource.
    bool addDependency(int waitingProcessID, int holdingProcessID);
    bool addDependency(const PCB& waitingProcess, const PCB& holdingProcess);
    bool removeDependency(int waitingProcessID, int holdingProcessID);

    DeadlockResult detectDeadlock() const;
    bool hasDeadlock() const;
    std::vector<int> getDeadlockedProcesses() const;

private:
    std::map<int, std::set<int>> waitForGraph;
};
