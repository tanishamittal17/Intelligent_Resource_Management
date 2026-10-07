#include "Database.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

bool expect(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << '\n';
        return true;
    }
    std::cout << "[FAIL] " << description << '\n';
    return false;
}

bool isEnvironmentSet(const char* name) {
    return std::getenv(name) != nullptr;
}

const char* typeName(TransactionType type) {
    switch (type) {
        case TransactionType::DEPOSIT: return "DEPOSIT";
        case TransactionType::WITHDRAWAL: return "WITHDRAWAL";
        case TransactionType::TRANSFER: return "TRANSFER";
        case TransactionType::LOAN_PAYMENT: return "LOAN_PAYMENT";
    }
    return "UNKNOWN";
}

} // namespace

int main() {
    std::unique_ptr<Database> database;
    try {
        database = std::make_unique<Database>();
    } catch (const std::exception& error) {
        std::cout << "[FAIL] Database object creation: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (!expect(database != nullptr, "Database object can be created")) {
        return EXIT_FAILURE;
    }

    const char* databaseUser = std::getenv("DB_USER");
    if (databaseUser == nullptr || databaseUser[0] == '\0' || !isEnvironmentSet("DB_PASSWORD")) {
        std::cout << "[FAIL] Connection test requires a non-empty DB_USER and a set DB_PASSWORD\n";
        return EXIT_FAILURE;
    }

    if (!database->connect()) {
        std::cout << "[FAIL] Database connection: " << database->getLastError() << '\n';
        return EXIT_FAILURE;
    }
    bool allPassed = expect(database->isConnected(), "Connection is established and reported as active");

    try {
        const QueryResult result = database->executeSelect("SELECT COUNT(*) FROM Accounts");
        const bool countIsValid = result.rows.size() == 1 && result.rows[0].size() == 1 &&
                                  std::stoll(result.rows[0][0]) >= 0;
        allPassed = expect(countIsValid, "SELECT COUNT(*) FROM Accounts returns a non-negative count") &&
                    allPassed;
    } catch (const std::exception& error) {
        std::cout << "[FAIL] Account count query: " << error.what() << '\n';
        allPassed = false;
    }

    const std::vector<Transaction> transactions = database->fetchTransactions();
    if (transactions.empty()) {
        std::cout << "[INFO] Transaction fetch detail: " << database->getLastError() << '\n';
    }
    allPassed = expect(!transactions.empty(), "Transactions table returns at least one transaction") &&
                allPassed;
    bool validTransactions = !transactions.empty();
    bool hasExpectedType = false;
    for (const Transaction& transaction : transactions) {
        validTransactions = transaction.transactionID > 0 && transaction.sourceAccountID > 0 &&
                            transaction.amount > 0.0 &&
                            (transaction.destinationAccountID == -1 || transaction.destinationAccountID > 0) &&
                            validTransactions;
        hasExpectedType = hasExpectedType || transaction.type == TransactionType::TRANSFER;
        std::cout << "Transaction " << transaction.transactionID
                  << " | " << typeName(transaction.type)
                  << " | source account " << transaction.sourceAccountID
                  << " | destination account ";
        if (transaction.destinationAccountID == -1) {
            std::cout << "N/A";
        } else {
            std::cout << transaction.destinationAccountID;
        }
        std::cout << " | amount " << transaction.amount
                  << " | priority " << transaction.priority << '\n';
    }
    allPassed = expect(validTransactions, "Fetched transaction fields contain valid values") && allPassed;
    allPassed = expect(hasExpectedType, "Seeded TRANSFER transaction type is present") && allPassed;
    allPassed = expect(database->getLastError().empty(), "Transaction rows were fetched without errors") &&
                allPassed;

    database->disconnect();
    allPassed = expect(!database->isConnected(), "Disconnect closes the database connection") && allPassed;

    return allPassed ? EXIT_SUCCESS : EXIT_FAILURE;
}
