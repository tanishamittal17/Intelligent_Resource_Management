cmake_minimum_required(VERSION 3.10)
project(IntelligentResourceManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The MySQL Server installation provides the C API header, import library, and DLL.
set(MYSQL_SERVER_ROOT "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH
    "MySQL Server installation used for the C API")
find_path(MYSQL_C_API_INCLUDE_DIR mysql.h HINTS "${MYSQL_SERVER_ROOT}/include")
find_library(MYSQL_C_API_LIBRARY NAMES libmysql mysqlclient
    HINTS "${MYSQL_SERVER_ROOT}/lib")
find_file(MYSQL_C_API_DLL NAMES libmysql.dll
    HINTS "${MYSQL_SERVER_ROOT}/lib" "${MYSQL_SERVER_ROOT}/bin")

set(MYSQL_C_API_AVAILABLE FALSE)
if(MYSQL_C_API_INCLUDE_DIR AND MYSQL_C_API_LIBRARY)
    set(MYSQL_C_API_AVAILABLE TRUE)
else()
    message(STATUS "MySQL C API development files not found; database targets are disabled. Set MYSQL_SERVER_ROOT or MYSQL_C_API_INCLUDE_DIR and MYSQL_C_API_LIBRARY.")
endif()

include_directories(include)

# Placeholder for future source files
set(SOURCES
    src/main.cpp
    src/transaction/PCB.cpp
    src/transaction/TransactionSimulator.cpp
    src/scheduler/Scheduler.cpp
    src/resource/ResourceManager.cpp
    src/resource/SynchronizationManager.cpp
    src/resource/DeadlockDetector.cpp
)

if(MYSQL_C_API_AVAILABLE)
    list(APPEND SOURCES
        src/db/Database.cpp
        src/db/TransactionSimulatorDatabase.cpp
        src/db/TransactionPipeline.cpp
    )
endif()

add_executable(IntelligentResourceManager ${SOURCES})
if(MYSQL_C_API_AVAILABLE)
    target_include_directories(IntelligentResourceManager PRIVATE "${MYSQL_C_API_INCLUDE_DIR}")
    target_link_libraries(IntelligentResourceManager PRIVATE "${MYSQL_C_API_LIBRARY}")
    if(MYSQL_C_API_DLL)
        add_custom_command(TARGET IntelligentResourceManager POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MYSQL_C_API_DLL}" "$<TARGET_FILE_DIR:IntelligentResourceManager>")
    endif()
endif()

# Basic PCB Tests
add_executable(test_pcb tests/test_pcb.cpp src/transaction/PCB.cpp)

add_executable(test_transaction_simulator
    tests/test_transaction_simulator.cpp
    src/transaction/TransactionSimulator.cpp
    src/scheduler/Scheduler.cpp
    src/resource/ResourceManager.cpp
    src/resource/SynchronizationManager.cpp
    src/transaction/PCB.cpp
)

# Baseline scheduling algorithm tests
add_executable(test_scheduler
    tests/test_scheduler.cpp
    src/scheduler/Scheduler.cpp
    src/transaction/PCB.cpp
)

add_executable(test_resource_manager
    tests/test_resource_manager.cpp
    src/resource/ResourceManager.cpp
    src/transaction/PCB.cpp
)

add_executable(test_synchronization
    tests/test_synchronization.cpp
    src/resource/SynchronizationManager.cpp
    src/transaction/PCB.cpp
)

add_executable(test_deadlock_detector
    tests/test_deadlock_detector.cpp
    src/resource/DeadlockDetector.cpp
    src/transaction/PCB.cpp
)

if(MYSQL_C_API_AVAILABLE)
    add_executable(test_database
        tests/test_database.cpp
        src/db/Database.cpp
    )
    target_include_directories(test_database PRIVATE "${MYSQL_C_API_INCLUDE_DIR}")
    target_link_libraries(test_database PRIVATE "${MYSQL_C_API_LIBRARY}")

    if(MYSQL_C_API_DLL)
        add_custom_command(TARGET test_database POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MYSQL_C_API_DLL}" "$<TARGET_FILE_DIR:test_database>")
    endif()

    add_executable(test_transaction_pipeline
        tests/test_transaction_pipeline.cpp
        src/db/TransactionPipeline.cpp
        src/db/Database.cpp
        src/transaction/TransactionSimulator.cpp
        src/scheduler/Scheduler.cpp
        src/resource/ResourceManager.cpp
        src/resource/SynchronizationManager.cpp
        src/transaction/PCB.cpp
    )
    target_include_directories(test_transaction_pipeline PRIVATE "${MYSQL_C_API_INCLUDE_DIR}")
    target_link_libraries(test_transaction_pipeline PRIVATE "${MYSQL_C_API_LIBRARY}")

    if(MYSQL_C_API_DLL)
        add_custom_command(TARGET test_transaction_pipeline POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MYSQL_C_API_DLL}" "$<TARGET_FILE_DIR:test_transaction_pipeline>")
    endif()
endif()

enable_testing()
add_test(NAME pcb_tests COMMAND test_pcb)
add_test(NAME transaction_simulator_tests COMMAND test_transaction_simulator)
add_test(NAME scheduler_tests COMMAND test_scheduler)
add_test(NAME resource_manager_tests COMMAND test_resource_manager)
add_test(NAME synchronization_tests COMMAND test_synchronization)
add_test(NAME deadlock_detector_tests COMMAND test_deadlock_detector)
if(MYSQL_C_API_AVAILABLE)
    add_test(NAME database_tests COMMAND test_database)
    add_test(NAME transaction_pipeline_tests COMMAND test_transaction_pipeline)
endif()

# Future: Add Google Test, MySQL connector paths, etc.
