#include "DeadlockDetector.hpp"

#include <functional>
#include <map>
#include <vector>

bool DeadlockDetector::addProcess(int processID) {
    return waitForGraph.emplace(processID, std::set<int>{}).second;
}

bool DeadlockDetector::addProcess(const PCB& process) {
    return addProcess(process.processID);
}

bool DeadlockDetector::removeProcess(int processID) {
    if (waitForGraph.erase(processID) == 0) {
        return false;
    }

    for (auto& process : waitForGraph) {
        process.second.erase(processID);
    }
    return true;
}

bool DeadlockDetector::addDependency(int waitingProcessID, int holdingProcessID) {
    auto waitingProcess = waitForGraph.find(waitingProcessID);
    if (waitingProcess == waitForGraph.end() || waitForGraph.find(holdingProcessID) == waitForGraph.end()) {
        return false;
    }

    return waitingProcess->second.insert(holdingProcessID).second;
}

bool DeadlockDetector::addDependency(const PCB& waitingProcess, const PCB& holdingProcess) {
    return addDependency(waitingProcess.processID, holdingProcess.processID);
}

bool DeadlockDetector::removeDependency(int waitingProcessID, int holdingProcessID) {
    auto waitingProcess = waitForGraph.find(waitingProcessID);
    if (waitingProcess == waitForGraph.end()) {
        return false;
    }
    return waitingProcess->second.erase(holdingProcessID) != 0;
}

DeadlockResult DeadlockDetector::detectDeadlock() const {
    // 0 = unvisited, 1 = on the current DFS path, 2 = fully explored.
    std::map<int, int> colors;
    std::map<int, std::size_t> pathPositions;
    std::vector<int> path;
    std::vector<int> cycle;

    std::function<bool(int)> visit = [&](int processID) {
        colors[processID] = 1;
        pathPositions[processID] = path.size();
        path.push_back(processID);

        for (int waitingOn : waitForGraph.at(processID)) {
            if (colors[waitingOn] == 0) {
                if (visit(waitingOn)) {
                    return true;
                }
            } else if (colors[waitingOn] == 1) {
                const std::size_t cycleStart = pathPositions.at(waitingOn);
                cycle.assign(path.begin() + cycleStart, path.end());
                return true;
            }
        }

        path.pop_back();
        pathPositions.erase(processID);
        colors[processID] = 2;
        return false;
    };

    for (const auto& process : waitForGraph) {
        if (colors[process.first] == 0 && visit(process.first)) {
            return DeadlockResult{DeadlockStatus::DEADLOCK_DETECTED, cycle};
        }
    }

    return DeadlockResult{DeadlockStatus::NO_DEADLOCK, {}};
}

bool DeadlockDetector::hasDeadlock() const {
    return detectDeadlock().hasDeadlock();
}

std::vector<int> DeadlockDetector::getDeadlockedProcesses() const {
    return detectDeadlock().processIDs;
}
