#include "PCB.hpp"
#include <iostream>

void assertTest(bool condition, const std::string& testName) {
    if (condition) {
        std::cout << "[PASS] " << testName << "\n";
    } else {
        std::cout << "[FAIL] " << testName << "\n";
        exit(1);
    }
}

int main() {
    std::cout << "Starting PCB Tests...\n\n";

    // Create a mock transaction
    Transaction t1 { 101, TransactionType::TRANSFER, 1, 2, 500.0, 3 };
    
    // Create PCB
    PCB pcb(1, t1, 0, 10);

    // Test 1: PCB creation and initial NEW state
    assertTest(pcb.processID == 1, "PCB Process ID matches");
    assertTest(pcb.transactionID == 101, "PCB Transaction ID matches");
    assertTest(pcb.state == ProcessState::NEW, "Initial state is NEW");
    assertTest(pcb.remainingTime == 10, "Initial remaining time equals burst time");

    // A database-style transaction can be represented as a PCB without MySQL access.
    const Transaction deposit{202, TransactionType::DEPOSIT, 1, -1, 125.50, 7};
    const PCB depositProcess = PCB::createFromTransaction(deposit);
    assertTest(depositProcess.processID > pcb.processID, "Transaction factory generates a process ID");
    assertTest(depositProcess.transactionID == deposit.transactionID,
               "Factory preserves the transaction ID");
    assertTest(depositProcess.transaction.type == TransactionType::DEPOSIT,
               "Factory creates a PCB for a DEPOSIT transaction");
    assertTest(depositProcess.transaction.amount == deposit.amount &&
               depositProcess.transaction.sourceAccountID == deposit.sourceAccountID &&
               depositProcess.transaction.destinationAccountID == deposit.destinationAccountID,
               "Factory preserves transaction details");
    assertTest(depositProcess.priority == deposit.priority,
               "Factory copies transaction priority to the PCB");
    assertTest(depositProcess.state == ProcessState::NEW,
               "Factory-created PCB starts in NEW state");
    assertTest(depositProcess.arrivalTime == 0 && depositProcess.burstTime > 0,
               "Factory uses a valid burst time and default arrival time");
    assertTest(depositProcess.remainingTime == depositProcess.burstTime,
               "Factory initializes remaining time to burst time");

    // Test 2: State transition
    pcb.changeState(ProcessState::READY);
    assertTest(pcb.state == ProcessState::READY, "State transition to READY");
    pcb.changeState(ProcessState::RUNNING);
    assertTest(pcb.state == ProcessState::RUNNING, "State transition to RUNNING");

    // Test 3: Remaining burst time update
    pcb.updateRemainingTime(3);
    assertTest(pcb.remainingTime == 7, "Remaining time updated (10 - 3 = 7)");
    pcb.updateRemainingTime(10); // over decrement
    assertTest(pcb.remainingTime == 0, "Remaining time correctly floored at 0");

    std::cout << "\nAll basic tests passed successfully!\n\n";

    std::cout << "Example PCB Output:\n";
    pcb.displayInfo();

    return 0;
}
