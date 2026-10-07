# C++ Core Engine - Transaction and PCB

This folder contains the core C++ simulation engine for financial transactions. 

## Process Control Block (PCB) Model

In an Operating System, a PCB represents the execution state of a process. For our simulator, each financial transaction (e.g., Deposit, Withdrawal, Transfer) is wrapped inside a PCB to allow the system to simulate OS-level scheduling algorithms.

### PCB Information
A PCB tracks the following:
- **Process ID & Transaction ID**: Unique identifiers tying the OS process to the financial operation.
- **Timing**: 
  - `Arrival Time`: When the transaction entered the system.
  - `Burst Time`: Total CPU cycles required to execute the transaction.
  - `Remaining Time`: Cycles left until completion.
- **Priority**: How important the process is.
- **Process State**: The current execution status.

### Process States
The PCB can move through the following standard OS states:
- **NEW**: Process created but not yet admitted to the ready queue.
- **READY**: Process waiting for CPU time.
- **RUNNING**: Process is currently executing on the CPU.
- **WAITING**: Process is blocked (e.g., waiting for an I/O operation or a resource lock).
- **COMPLETED**: Process execution finished successfully.
- **FAILED**: Process execution aborted (e.g., due to a deadlock or a failed business logic like insufficient funds).

## Baseline CPU Schedulers

The core engine provides FCFS, Round Robin, and non-preemptive Priority scheduling. Each scheduler accepts the existing `PCB` objects and returns completion, waiting, and turnaround times in the same order as the input list. A scheduling run resets each PCB's remaining time and state before it begins.

Priority scheduling chooses the largest `PCB::priority` value first. When priorities are equal, it chooses the earlier arrival; input order breaks any remaining tie. Round Robin uses the configured positive time quantum and a FIFO ready queue.

## Basic Resource Management

The resource manager tracks named resources by ID, including their total and available units. A process allocates units through the manager; successful allocations are recorded against that PCB's process ID and reduce availability. A process can release only units it previously allocated, which returns those units to availability. Allocation and release requests with invalid quantities or insufficient holdings are rejected.

## Basic Synchronization

A mutex provides exclusive access to a shared resource: one PCB locks it, and only that owner can unlock it. A counting semaphore tracks a bounded number of available permits; processes wait to acquire a permit and signal to return one. These simple simulation objects let processes coordinate access to shared resources.

## Deadlock Detection

A deadlock occurs when processes wait on one another in a circular chain and none can proceed. The detector represents each process as a node in a wait-for graph and adds an edge from a waiting process to the process holding the resource it needs. A depth-first search looks for a cycle; a cycle is reported as a deadlock along with the process IDs in that cycle. Tests cover empty and acyclic graphs, two- and three-process cycles, cycle identification, and removal of a dependency or process. This milestone detects deadlocks only; it does not implement recovery or Banker's algorithm.

## C++ MySQL Connection

The database layer uses the MySQL Server C API (`mysql_init`, `mysql_real_connect`, and the query/result functions). CMake looks for `mysql.h`, the MySQL client import library, and the runtime DLL under `MYSQL_SERVER_ROOT`. The default is `C:/Program Files/MySQL/MySQL Server 8.0`; override it when MySQL Server is installed elsewhere. If the header or import library is missing, CMake keeps the engine and other tests buildable and disables the database target with a status message.

The connection reads credentials from environment variables and never prints the password. `DB_USER` and `DB_PASSWORD` are required. `DB_HOST` defaults to `localhost`, `DB_PORT` defaults to `3306`, and `DB_NAME` defaults to `resource_management_db`.

For Windows/MSYS2 UCRT64, configure with the installed MySQL Server files and build the database test:

```powershell
cmake -S core_engine -B core_engine/build -DMYSQL_SERVER_ROOT="C:/Program Files/MySQL/MySQL Server 8.0"
cmake --build core_engine/build --target test_database
$env:PATH = 'C:\Program Files\MySQL\MySQL Server 8.0\bin;C:\Program Files\MySQL\MySQL Server 8.0\lib;' + $env:PATH
$env:DB_USER = Read-Host 'DB_USER'
$secret = Read-Host 'DB_PASSWORD' -AsSecureString
$env:DB_PASSWORD = [System.Net.NetworkCredential]::new('', $secret).Password
ctest --test-dir core_engine/build --output-on-failure -R database_tests
```

With CMake unavailable, the equivalent direct UCRT64 build is:

```powershell
$mysqlRoot = 'C:\Program Files\MySQL\MySQL Server 8.0'
g++ -std=c++17 -Wall -Wextra -Wpedantic -I core_engine/include -I "$mysqlRoot\include" core_engine/tests/test_database.cpp core_engine/src/db/Database.cpp "$mysqlRoot\lib\libmysql.lib" -o core_engine/build/test_database.exe
```

Add `$mysqlRoot\bin` and `$mysqlRoot\lib` to `PATH` before running `core_engine/build/test_database.exe`. The test runs `SELECT COUNT(*) FROM Accounts`, reads the seeded transaction records, and then disconnects.

## Reading Transactions from MySQL

C++ reads transaction records from MySQL and converts them into the existing `Transaction` model. `Database::fetchTransactions()` selects the transaction ID, type, source and destination account IDs, amount, and priority in transaction ID order. A nullable destination account is represented as `-1` in the model.

Each database transaction record can now be represented as an OS-style PCB/process with `PCB::createFromTransaction()`. The factory preserves the transaction details and priority, assigns a process ID, initializes the PCB in the `NEW` state, and uses simple default timing values so the process can be passed to the existing CPU schedulers.

## Transaction Simulation Flow

`TransactionSimulator` connects transaction records to process creation and CPU scheduling. It accepts `Transaction` objects (including those returned by `Database::fetchTransactions()`), creates and stores one PCB per transaction, and delegates FCFS, Round Robin, or Priority scheduling to the existing scheduler classes. Its unit tests use manually created transactions and do not need a MySQL connection.

MySQL Server and the existing `resource_management_db` database are already installed. This layer uses that database and the existing `Accounts` table; it does not create or modify the database or schema.
