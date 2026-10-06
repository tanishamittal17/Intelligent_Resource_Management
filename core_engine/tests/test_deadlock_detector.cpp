#include "DeadlockDetector.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

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

std::set<int> asSet(const std::vector<int>& processIDs) {
    return std::set<int>(processIDs.begin(), processIDs.end());
}

} // namespace

int main() {
    DeadlockDetector emptyGraph;
    const DeadlockResult emptyResult = emptyGraph.detectDeadlock();
    expect(emptyResult.status == DeadlockStatus::NO_DEADLOCK && emptyResult.processIDs.empty(),
           "Empty graph has no deadlock");

    DeadlockDetector singleProcess;
    expect(singleProcess.addProcess(1), "Single process can be added");
    expect(singleProcess.detectDeadlock().status == DeadlockStatus::NO_DEADLOCK,
           "Single process without dependencies has no deadlock");

    DeadlockDetector chain;
    expect(chain.addProcess(1) && chain.addProcess(2) && chain.addProcess(3),
           "Processes can be added to the wait-for graph");
    expect(chain.addDependency(1, 2) && chain.addDependency(2, 3),
           "A non-circular wait chain can be added");
    expect(chain.detectDeadlock().status == DeadlockStatus::NO_DEADLOCK,
           "P1 to P2 to P3 chain has no deadlock");

    DeadlockDetector twoProcessCycle;
    twoProcessCycle.addProcess(1);
    twoProcessCycle.addProcess(2);
    twoProcessCycle.addDependency(1, 2);
    twoProcessCycle.addDependency(2, 1);
    const DeadlockResult twoProcessResult = twoProcessCycle.detectDeadlock();
    expect(twoProcessResult.status == DeadlockStatus::DEADLOCK_DETECTED && twoProcessResult.hasDeadlock(),
           "Two-process cycle is reported as a deadlock");
    expect(asSet(twoProcessResult.processIDs) == std::set<int>({1, 2}),
           "Two-process deadlock identifies both processes");

    DeadlockDetector threeProcessCycle;
    threeProcessCycle.addProcess(1);
    threeProcessCycle.addProcess(2);
    threeProcessCycle.addProcess(3);
    threeProcessCycle.addDependency(1, 2);
    threeProcessCycle.addDependency(2, 3);
    threeProcessCycle.addDependency(3, 1);
    const DeadlockResult threeProcessResult = threeProcessCycle.detectDeadlock();
    expect(threeProcessResult.hasDeadlock(), "Three-process circular wait is detected");
    expect(asSet(threeProcessResult.processIDs) == std::set<int>({1, 2, 3}),
           "Three-process deadlock identifies every process in the cycle");

    expect(threeProcessCycle.removeDependency(3, 1), "A wait dependency can be removed");
    expect(!threeProcessCycle.hasDeadlock() && threeProcessCycle.getDeadlockedProcesses().empty(),
           "Removing a cycle edge clears the deadlock report");

    DeadlockDetector processRemoval;
    processRemoval.addProcess(10);
    processRemoval.addProcess(11);
    processRemoval.addDependency(10, 11);
    processRemoval.addDependency(11, 10);
    expect(processRemoval.removeProcess(11) && !processRemoval.hasDeadlock(),
           "Removing a process also removes dependencies to and from it");

    std::cout << "\nDeadlock detector tests: " << testsPassed << " passed, " << testsFailed << " failed.\n";
    return testsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
