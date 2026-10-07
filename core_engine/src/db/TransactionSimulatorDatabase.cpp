#include "Database.hpp"
#include "TransactionSimulator.hpp"

bool TransactionSimulator::updateDatabaseStatuses(Database& database) const {
    bool allUpdatesSucceeded = true;
    for (const PCB& process : getProcesses()) {
        const char* status = getTerminalDatabaseStatus(process.state);
        if (status != nullptr && !database.updateTransactionStatus(process.transactionID, status)) {
            allUpdatesSucceeded = false;
        }
    }
    return allUpdatesSucceeded;
}
