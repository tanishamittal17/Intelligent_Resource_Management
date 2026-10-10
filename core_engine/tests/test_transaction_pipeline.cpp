#include "Database.hpp"
#include "TransactionPipeline.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

bool credentialsAvailable() {
    const char* user = std::getenv("DB_USER");
    return user != nullptr && user[0] != '\0' && std::getenv("DB_PASSWORD") != nullptr;
}

class StatusRestoreGuard {
public:
    StatusRestoreGuard(Database& database, std::map<int, std::string> originalStatuses)
        : database(database), originalStatuses(std::move(originalStatuses)) {}

    ~StatusRestoreGuard() {
        if (active) {
            restore();
        }
    }

    bool restore() {
        bool allRestored = true;
        for (const auto& transaction : originalStatuses) {
            if (!database.updateTransactionStatus(transaction.first, transaction.second)) {
                allRestored = false;
            }
        }
        if (allRestored) {
            active = false;
        }
        return allRestored;
    }

private:
    Database& database;
    std::map<int, std::string> originalStatuses;
    bool active = true;
};

std::map<int, std::string> readStatuses(Database& database) {
    const QueryResult rows = database.executeSelect(
        "SELECT TransactionID, Status FROM Transactions ORDER BY TransactionID");
    std::map<int, std::string> statuses;
    for (const auto& row : rows.rows) {
        if (row.size() != 2) {
            throw std::runtime_error("Unexpected transaction status query result");
        }
        statuses.emplace(std::stoi(row[0]), row[1]);
    }
    return statuses;
}

} // namespace

int main() {
    if (!credentialsAvailable()) {
        std::cout << "[SKIP] Live pipeline integration requires DB_USER and DB_PASSWORD\n";
        return 77;
    }

    Database database;
    if (!database.connect()) {
        std::cout << "[FAIL] MySQL connection: " << database.getLastError() << '\n';
        return EXIT_FAILURE;
    }
    expect(database.isConnected(), "Connected to the existing MySQL database");

    try {
        const std::map<int, std::string> originalStatuses = readStatuses(database);
        if (originalStatuses.empty()) {
            std::cout << "[FAIL] No seeded Transactions rows are available\n";
            return EXIT_FAILURE;
        }
        StatusRestoreGuard restoreStatuses(database, originalStatuses);

        TransactionPipeline pipeline;
        const bool pipelineSucceeded = pipeline.runDatabaseBackedSimulation();
        if (!pipelineSucceeded) {
            std::cout << "[FAIL] Database-backed simulation: " << pipeline.getLastError() << '\n';
        }
        expect(pipelineSucceeded, "Database-backed transaction simulation completes");

        const std::map<int, std::string> simulatedStatuses = readStatuses(database);
        bool allTransactionsCompleted = simulatedStatuses.size() == originalStatuses.size();
        for (const auto& transaction : originalStatuses) {
            const auto simulated = simulatedStatuses.find(transaction.first);
            allTransactionsCompleted = allTransactionsCompleted &&
                simulated != simulatedStatuses.end() && simulated->second == "COMPLETED";
        }
        expect(allTransactionsCompleted,
               "Each seeded transaction ID has the expected COMPLETED status");

        const bool restored = restoreStatuses.restore();
        expect(restored, "Original transaction statuses are restored");
        const std::map<int, std::string> restoredStatuses = readStatuses(database);
        expect(restoredStatuses == originalStatuses,
               "Database confirms all original transaction statuses are restored");
    } catch (const std::exception& error) {
        std::cout << "[FAIL] Pipeline integration test error: " << error.what() << '\n';
        ++testsFailed;
    }

    database.disconnect();
    expect(!database.isConnected(), "Database connection is closed");
    std::cout << "\nTransaction pipeline integration: " << testsPassed << " passed, "
              << testsFailed << " failed.\n";
    return testsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
