#include "PCB.hpp"

int PCB::nextProcessID = 1;

PCB::PCB(int pID, Transaction txn, int arrTime, int bTime)
    : processID(pID), 
      transactionID(txn.transactionID), 
      arrivalTime(arrTime),
      burstTime(bTime), 
      remainingTime(bTime), 
      priority(txn.priority),
      state(ProcessState::NEW), 
      transaction(txn) {
    if (pID >= nextProcessID) {
        nextProcessID = pID + 1;
    }
}

PCB PCB::createFromTransaction(const Transaction& transaction, int arrivalTime, int burstTime) {
    return PCB(nextProcessID++, transaction, arrivalTime, burstTime);
}

void PCB::displayInfo() const {
    std::cout << "--- PCB Info ---\n"
              << "Process ID:     " << processID << "\n"
              << "Transaction ID: " << transactionID << "\n"
              << "Priority:       " << priority << "\n"
              << "Arrival Time:   " << arrivalTime << "\n"
              << "Burst Time:     " << burstTime << "\n"
              << "Remaining Time: " << remainingTime << "\n"
              << "State:          ";
              
    switch (state) {
        case ProcessState::NEW:       std::cout << "NEW\n"; break;
        case ProcessState::READY:     std::cout << "READY\n"; break;
        case ProcessState::RUNNING:   std::cout << "RUNNING\n"; break;
        case ProcessState::WAITING:   std::cout << "WAITING\n"; break;
        case ProcessState::COMPLETED: std::cout << "COMPLETED\n"; break;
        case ProcessState::FAILED:    std::cout << "FAILED\n"; break;
    }
    std::cout << "----------------\n";
}

void PCB::changeState(ProcessState newState) {
    state = newState;
}

void PCB::updateRemainingTime(int timePassed) {
    if (timePassed < 0) return;
    remainingTime -= timePassed;
    if (remainingTime < 0) remainingTime = 0;
}
