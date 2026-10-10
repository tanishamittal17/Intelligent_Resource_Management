#pragma once

#include <string>

class TransactionPipeline {
public:
    // Loads database transactions, simulates them, and writes terminal statuses.
    bool runDatabaseBackedSimulation();

    const std::string& getLastError() const noexcept;

private:
    std::string lastError;
};
