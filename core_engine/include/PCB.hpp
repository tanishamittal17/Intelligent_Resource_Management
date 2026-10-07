#pragma once

#include "Transaction.hpp"
#include <iostream>

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    WAITING,
    COMPLETED,
    FAILED
};

class PCB {
public:
    int processID;
    int transactionID;
    int arrivalTime;
    int burstTime;
    int remainingTime;
    int priority;
    ProcessState state;
    Transaction transaction;

    // Constructor to create a transaction process
    PCB(int pID, Transaction txn, int arrTime, int bTime);

    // Creates a PCB with a generated process ID and simple default timing values.
    static PCB createFromTransaction(const Transaction& transaction,
                                     int arrivalTime = 0,
                                     int burstTime = 1);

    // Display its PCB information
    void displayInfo() const;

    // Change process state
    void changeState(ProcessState newState);

    // Update remaining burst time
    void updateRemainingTime(int timePassed);

private:
    static int nextProcessID;
};
