#include "Database.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::string getEnvironmentValue(const char* variableName, const std::string& defaultValue = {}) {
    const char* value = std::getenv(variableName);
    return value == nullptr ? defaultValue : std::string(value);
}

unsigned int getPort() {
    const std::string value = getEnvironmentValue("DB_PORT", "3306");
    std::size_t parsedLength = 0;
    unsigned long port = 0;

    try {
        port = std::stoul(value, &parsedLength);
    } catch (const std::exception&) {
        throw std::runtime_error("DB_PORT must be a number between 1 and 65535");
    }

    if (parsedLength != value.size() || port == 0 || port > 65535) {
        throw std::runtime_error("DB_PORT must be a number between 1 and 65535");
    }
    return static_cast<unsigned int>(port);
}

bool isSelectQuery(const std::string& query) {
    std::size_t first = 0;
    while (first < query.size() && std::isspace(static_cast<unsigned char>(query[first]))) {
        ++first;
    }

    const std::string keyword = "SELECT";
    if (query.size() - first < keyword.size()) {
        return false;
    }

    for (std::size_t index = 0; index < keyword.size(); ++index) {
        if (std::toupper(static_cast<unsigned char>(query[first + index])) != keyword[index]) {
            return false;
        }
    }

    const std::size_t afterKeyword = first + keyword.size();
    return afterKeyword == query.size() ||
           std::isspace(static_cast<unsigned char>(query[afterKeyword])) || query[afterKeyword] == '(';
}

struct ResultDeleter {
    void operator()(MYSQL_RES* result) const {
        if (result != nullptr) {
            mysql_free_result(result);
        }
    }
};

struct StatementDeleter {
    void operator()(MYSQL_STMT* statement) const {
        if (statement != nullptr) {
            mysql_stmt_close(statement);
        }
    }
};

bool isValidTransactionStatus(const std::string& status) {
    return status == "PENDING" || status == "RUNNING" || status == "COMPLETED" ||
           status == "FAILED" || status == "ROLLED_BACK";
}

} // namespace

namespace {

int parseInt(const std::string& value, const char* columnName) {
    std::size_t parsedLength = 0;
    long parsedValue = 0;
    try {
        parsedValue = std::stol(value, &parsedLength);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("Invalid integer in Transactions.") + columnName);
    }
    if (parsedLength != value.size() || parsedValue < std::numeric_limits<int>::min() ||
        parsedValue > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string("Invalid integer in Transactions.") + columnName);
    }
    return static_cast<int>(parsedValue);
}

TransactionType parseTransactionType(const std::string& value) {
    if (value == "DEPOSIT") return TransactionType::DEPOSIT;
    if (value == "WITHDRAWAL") return TransactionType::WITHDRAWAL;
    if (value == "TRANSFER") return TransactionType::TRANSFER;
    if (value == "LOAN_PAYMENT") return TransactionType::LOAN_PAYMENT;
    throw std::runtime_error("Unknown TransactionType returned by MySQL: " + value);
}

} // namespace

Database::Database() = default;

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    disconnect();
    lastError.clear();

    const std::string user = getEnvironmentValue("DB_USER");
    const std::string password = getEnvironmentValue("DB_PASSWORD");
    if (user.empty() || std::getenv("DB_PASSWORD") == nullptr) {
        lastError = "DB_USER must be non-empty and DB_PASSWORD must be set in the environment";
        return false;
    }

    const std::string host = getEnvironmentValue("DB_HOST", "localhost");
    const std::string databaseName = getEnvironmentValue("DB_NAME", "resource_management_db");
    if (host.empty() || databaseName.empty()) {
        lastError = "DB_HOST and DB_NAME must not be empty";
        return false;
    }

    try {
        const unsigned int port = getPort();
        MYSQL* initializedConnection = mysql_init(nullptr);
        if (initializedConnection == nullptr) {
            lastError = "mysql_init failed to allocate a connection handle";
            return false;
        }

        MYSQL* connected = mysql_real_connect(initializedConnection, host.c_str(), user.c_str(),
                                               password.c_str(), databaseName.c_str(), port,
                                               nullptr, 0);
        if (connected == nullptr) {
            lastError = "MySQL connection failed: " + std::string(mysql_error(initializedConnection)) +
                        " (error code " + std::to_string(mysql_errno(initializedConnection)) + ")";
            mysql_close(initializedConnection);
            return false;
        }

        connection = connected;
        return true;
    } catch (const std::exception& error) {
        lastError = "Database connection failed: " + std::string(error.what());
        return false;
    }
}

bool Database::isConnected() const noexcept {
    return connection != nullptr;
}

void Database::disconnect() noexcept {
    if (connection != nullptr) {
        mysql_close(connection);
        connection = nullptr;
    }
}

QueryResult Database::executeSelect(const std::string& query) {
    if (!isConnected()) {
        lastError = "Cannot execute SELECT: database is not connected";
        throw std::runtime_error(lastError);
    }
    if (!isSelectQuery(query)) {
        lastError = "Only SELECT statements are allowed by executeSelect";
        throw std::invalid_argument(lastError);
    }

    if (mysql_query(connection, query.c_str()) != 0) {
        lastError = "SELECT query failed: " + std::string(mysql_error(connection)) +
                    " (error code " + std::to_string(mysql_errno(connection)) + ")";
        throw std::runtime_error(lastError);
    }

    std::unique_ptr<MYSQL_RES, ResultDeleter> result(mysql_store_result(connection));
    if (result == nullptr) {
        lastError = "Could not store SELECT result: " + std::string(mysql_error(connection)) +
                    " (error code " + std::to_string(mysql_errno(connection)) + ")";
        throw std::runtime_error(lastError);
    }

    QueryResult queryResult;
    const unsigned int columnCount = mysql_num_fields(result.get());
    MYSQL_ROW mysqlRow = nullptr;
    while ((mysqlRow = mysql_fetch_row(result.get())) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result.get());
        if (lengths == nullptr) {
            lastError = "Could not read SELECT result row: " + std::string(mysql_error(connection));
            throw std::runtime_error(lastError);
        }

        std::vector<std::string> values;
        values.reserve(columnCount);
        for (unsigned int column = 0; column < columnCount; ++column) {
            if (mysqlRow[column] == nullptr) {
                values.emplace_back();
            } else {
                values.emplace_back(mysqlRow[column], lengths[column]);
            }
        }
        queryResult.rows.push_back(std::move(values));
    }

    if (mysql_errno(connection) != 0) {
        lastError = "Could not finish reading SELECT result: " + std::string(mysql_error(connection)) +
                    " (error code " + std::to_string(mysql_errno(connection)) + ")";
        throw std::runtime_error(lastError);
    }

    lastError.clear();
    return queryResult;
}

std::vector<Transaction> Database::fetchTransactions() {
    std::vector<Transaction> transactions;
    if (!isConnected()) {
        lastError = "Cannot fetch transactions: database is not connected";
        return transactions;
    }

    try {
        const QueryResult result = executeSelect(
            "SELECT TransactionID, TransactionType, SourceAccountID, DestinationAccountID, Amount, Priority "
            "FROM Transactions ORDER BY TransactionID");
        transactions.reserve(result.rows.size());
        for (const auto& row : result.rows) {
            if (row.size() != 6) {
                throw std::runtime_error("Unexpected column count returned for a transaction");
            }
            std::size_t amountLength = 0;
            double amount = 0.0;
            try {
                amount = std::stod(row[4], &amountLength);
            } catch (const std::exception&) {
                throw std::runtime_error("Invalid Amount returned for a transaction");
            }
            if (amountLength != row[4].size()) {
                throw std::runtime_error("Invalid Amount returned for a transaction");
            }

            transactions.push_back(Transaction{
                parseInt(row[0], "TransactionID"),
                parseTransactionType(row[1]),
                parseInt(row[2], "SourceAccountID"),
                row[3].empty() ? -1 : parseInt(row[3], "DestinationAccountID"),
                amount,
                parseInt(row[5], "Priority")
            });
        }
        return transactions;
    } catch (const std::exception& error) {
        // executeSelect supplies a more specific MySQL error when the query fails.
        if (lastError.empty()) {
            lastError = std::string("Could not fetch transactions: ") + error.what();
        }
        return {};
    }
}

bool Database::updateTransactionStatus(int transactionId, const std::string& status) {
    if (!isConnected()) {
        lastError = "Cannot update transaction status: database is not connected";
        return false;
    }
    if (transactionId <= 0) {
        lastError = "Transaction ID must be a positive integer";
        return false;
    }
    if (!isValidTransactionStatus(status)) {
        lastError = "Invalid transaction status; allowed values are PENDING, RUNNING, COMPLETED, FAILED, and ROLLED_BACK";
        return false;
    }

    MYSQL_STMT* rawStatement = mysql_stmt_init(connection);
    if (rawStatement == nullptr) {
        lastError = "Could not initialize MySQL status update statement";
        return false;
    }
    std::unique_ptr<MYSQL_STMT, StatementDeleter> statement(rawStatement);

    const char* query = "UPDATE Transactions SET Status = ? WHERE TransactionID = ?";
    if (mysql_stmt_prepare(statement.get(), query,
                           static_cast<unsigned long>(std::strlen(query))) != 0) {
        lastError = "Could not prepare transaction status update: " +
                    std::string(mysql_stmt_error(statement.get())) + " (error code " +
                    std::to_string(mysql_stmt_errno(statement.get())) + ")";
        return false;
    }

    unsigned long statusLength = static_cast<unsigned long>(status.size());
    int boundTransactionId = transactionId;
    MYSQL_BIND parameters[2]{};
    parameters[0].buffer_type = MYSQL_TYPE_STRING;
    parameters[0].buffer = const_cast<char*>(status.c_str());
    parameters[0].buffer_length = statusLength;
    parameters[0].length = &statusLength;
    parameters[1].buffer_type = MYSQL_TYPE_LONG;
    parameters[1].buffer = &boundTransactionId;
    parameters[1].is_unsigned = 0;

    if (mysql_stmt_bind_param(statement.get(), parameters) != 0) {
        lastError = "Could not bind transaction status update parameters: " +
                    std::string(mysql_stmt_error(statement.get())) + " (error code " +
                    std::to_string(mysql_stmt_errno(statement.get())) + ")";
        return false;
    }
    if (mysql_stmt_execute(statement.get()) != 0) {
        lastError = "Transaction status update failed: " +
                    std::string(mysql_stmt_error(statement.get())) + " (error code " +
                    std::to_string(mysql_stmt_errno(statement.get())) + ")";
        return false;
    }

    // A zero affected-row count is also a successful statement when the stored
    // status already matches the requested value.
    lastError.clear();
    return true;
}

const std::string& Database::getLastError() const noexcept {
    return lastError;
}
