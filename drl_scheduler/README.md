# Python Transaction Scheduling Environment

This package contains a small, deterministic `SchedulingEnv` and a beginner-friendly PyTorch Deep Q-Network (DQN) agent. It does not connect to MySQL or change the C++ engine.

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

## Install and run on Windows

From the project root in PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r drl_scheduler\requirements.txt
python -m unittest discover -s drl_scheduler/tests -v
```

The only third-party dependency is PyTorch. Choose the PyTorch wheel appropriate
for your machine if you need a specific CPU or CUDA build; the requirements file
uses the standard `torch` package.

## Use the DQN agent

The agent reads the observation length and transaction count from an environment.
Its output is one Q-value per possible action position. Before selecting an
action, it reads each transaction's completion flag and masks action positions
that are no longer in the ready queue. Exploration can be enabled with
epsilon-greedy selection; pass `explore=False` to select the highest-valued
eligible action.

```python
from drl_scheduler.agent import DQNAgent
from drl_scheduler.env import SchedulingEnv

env = SchedulingEnv()
agent = DQNAgent.from_environment(env, seed=7)
observation = env.reset()
action = agent.select_action(observation)  # ready-queue index
next_observation, reward, terminated, truncated, info = env.step(action)
agent.remember(observation, action, reward, next_observation, terminated, truncated)
loss = agent.train_step()  # None until a full replay batch is available
```

The replay buffer stores past transitions. Training uses a target network and
the DQN Bellman target; terminal and truncated transitions do not bootstrap.
This milestone provides the agent implementation and unit tests, not a long
training run or a trained policy.

## Run tests

From the project root, run:

```powershell
python -m unittest discover -s drl_scheduler/tests -v
```

Install PyTorch using the commands above before running the complete suite.

## Current limitations

This is a compact scheduling model with one CPU executing one unit per action. It is not a Gymnasium environment, does not load transactions from MySQL, and does not model resource allocation, synchronization, or financial balances. The DQN is a learning implementation; a useful trained policy still requires an explicitly configured training run.
