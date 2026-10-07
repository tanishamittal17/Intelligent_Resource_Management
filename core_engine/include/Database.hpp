#pragma once

#include "Transaction.hpp"

#include <mysql.h>

#include <string>
#include <vector>

struct QueryResult {
    std::vector<std::vector<std::string>> rows;
};

class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool connect();
    bool isConnected() const noexcept;
    void disconnect() noexcept;

    // Executes a read-only SELECT and returns its rows as strings.
    QueryResult executeSelect(const std::string& query);

    // Reads transaction rows and converts them to the existing Transaction model.
    std::vector<Transaction> fetchTransactions();

    const std::string& getLastError() const noexcept;

private:
    MYSQL* connection = nullptr;
    std::string lastError;
};
