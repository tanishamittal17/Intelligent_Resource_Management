# Python Transaction Scheduling Environment

This milestone adds a small, deterministic `SchedulingEnv` for exploring transaction scheduling before connecting it to a DRL agent. It uses only the Python standard library. It does not connect to MySQL, train an agent, or require PyTorch or Gymnasium.

## Workload and actions

The default workload contains three transactions. Each process tracks its transaction ID, burst time, priority, remaining time, waiting time, and completion status. You can pass a custom non-empty list of mappings with `transaction_id`, `burst_time`, and `priority` when creating an environment.

An action is an integer index into the current ready queue. The queue contains unfinished transactions in their original workload order. For example, action `0` runs the first unfinished transaction for one simulated time unit. The action mapping is recalculated after a transaction completes. An invalid action returns an `invalid_action` info flag and a penalty without changing the simulated clock or transaction data.

## Observation and episode behavior

`reset()` returns a tuple of numbers. Its fixed length for a workload is `1 + 6 * transaction_count`: elapsed simulated time, followed for each transaction by its ID, burst time, priority, remaining time, waiting time, and numeric completion flag. This representation can later be converted directly to a PyTorch tensor.

`step(action)` returns `(observation, reward, terminated, truncated, info)`. Every valid execution decreases the selected transaction's remaining time by one and increases each other ready transaction's waiting time by one. The episode terminates when all transactions complete. It truncates only when the configured maximum number of decisions is reached first, so an agent cannot run forever.

The simple reward is:

```text
-0.1 for each valid execution step
-0.01 for each other ready transaction waiting during that step
+1.0 when the selected transaction completes
-1.0 for an invalid action
```

Invalid actions count toward the maximum decision limit but do not change process state or simulated time. Repeated calls after termination or truncation return the same observation and done flags.

## Run tests

From the project root, run:

```powershell
python -m unittest discover -s drl_scheduler/tests -v
```

No package installation is needed for the environment or tests.

## Current limitations

This is a compact scheduling model with one CPU executing one unit per action. It is not yet a Gymnasium environment, does not load transactions from MySQL, and does not model resource allocation, synchronization, financial balances, or a trained DRL agent.
