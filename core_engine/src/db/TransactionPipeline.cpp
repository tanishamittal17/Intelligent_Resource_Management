#include "TransactionPipeline.hpp"

#include "Database.hpp"
#include "TransactionSimulator.hpp"

const std::string& TransactionPipeline::getLastError() const noexcept {
    return lastError;
}

bool TransactionPipeline::runDatabaseBackedSimulation() {
    lastError.clear();

    Database database;
    if (!database.connect()) {
        lastError = "Could not connect to MySQL: " + database.getLastError();
        return false;
    }

    const std::vector<Transaction> transactions = database.fetchTransactions();
    if (transactions.empty()) {
        lastError = database.getLastError().empty()
            ? "No transactions are available to simulate"
            : "Could not load transactions: " + database.getLastError();
        return false;
    }

    TransactionSimulator simulator;
    if (!simulator.runEndToEndSimulation(transactions)) {
        const SimulationSummary summary = simulator.getSimulationSummary();
        lastError = "Simulation did not complete successfully (completed " +
                    std::to_string(summary.completedTransactions) + " of " +
                    std::to_string(summary.totalTransactions) + ")";
        return false;
    }

    for (const PCB& process : simulator.getProcesses()) {
        const char* status = TransactionSimulator::getTerminalDatabaseStatus(process.state);
        if (status != nullptr && !database.updateTransactionStatus(process.transactionID, status)) {
            lastError = "Could not update status for transaction " +
                        std::to_string(process.transactionID) + ": " + database.getLastError();
            return false;
        }
    }

    return true;
}
