"""A small, deterministic transaction scheduling environment.

The environment uses only Python's standard library. Actions choose a transaction
from the current ready queue and run it for one simulated time unit.
"""

from dataclasses import dataclass
from typing import Iterable, Optional


@dataclass
class TransactionProcess:
    """Mutable process state for one simulated transaction."""

    transaction_id: int
    burst_time: int
    priority: int
    remaining_time: int
    waiting_time: int = 0
    completed: bool = False


class SchedulingEnv:
    """Scheduling environment with a fixed workload and numerical observations."""

    DEFAULT_WORKLOAD = (
        {"transaction_id": 1001, "burst_time": 3, "priority": 2},
        {"transaction_id": 1002, "burst_time": 2, "priority": 5},
        {"transaction_id": 1003, "burst_time": 1, "priority": 1},
    )

    STEP_PENALTY = 0.1
    WAITING_PENALTY_PER_PROCESS = 0.01
    COMPLETION_REWARD = 1.0
    INVALID_ACTION_PENALTY = 1.0

    def __init__(
        self,
        workload: Optional[Iterable[dict]] = None,
        max_episode_steps: int = 100,
    ) -> None:
        """Create an environment.

        A custom workload is a sequence of mappings with ``transaction_id``,
        ``burst_time``, and ``priority`` keys. The workload is copied on reset.
        """
        if isinstance(max_episode_steps, bool) or not isinstance(max_episode_steps, int):
            raise TypeError("max_episode_steps must be an integer")
        if max_episode_steps <= 0:
            raise ValueError("max_episode_steps must be positive")

        supplied_workload = self.DEFAULT_WORKLOAD if workload is None else tuple(workload)
        if not supplied_workload:
            raise ValueError("workload must contain at least one transaction")

        self._initial_workload = []
        seen_ids = set()
        for item in supplied_workload:
            try:
                transaction_id = item["transaction_id"]
                burst_time = item["burst_time"]
                priority = item["priority"]
            except (KeyError, TypeError) as error:
                raise ValueError(
                    "each workload item needs transaction_id, burst_time, and priority"
                ) from error

            for name, value in (
                ("transaction_id", transaction_id),
                ("burst_time", burst_time),
                ("priority", priority),
            ):
                if isinstance(value, bool) or not isinstance(value, int):
                    raise TypeError(f"{name} must be an integer")
            if transaction_id in seen_ids:
                raise ValueError("transaction_id values must be unique")
            if burst_time <= 0:
                raise ValueError("burst_time must be positive")
            seen_ids.add(transaction_id)
            self._initial_workload.append((transaction_id, burst_time, priority))

        self.max_episode_steps = max_episode_steps
        self.transactions = []
        self.elapsed_time = 0
        self._decision_count = 0
        self._terminated = False
        self._truncated = False
        self.reset()

    def reset(self) -> tuple[float, ...]:
        """Start a new episode and return its initial observation."""
        self.transactions = [
            TransactionProcess(
                transaction_id=transaction_id,
                burst_time=burst_time,
                priority=priority,
                remaining_time=burst_time,
            )
            for transaction_id, burst_time, priority in self._initial_workload
        ]
        self.elapsed_time = 0
        self._decision_count = 0
        self._terminated = False
        self._truncated = False
        return self.get_observation()

    def _ready_transactions(self) -> list[TransactionProcess]:
        return [transaction for transaction in self.transactions if not transaction.completed]

    def get_observation(self) -> tuple[float, ...]:
        """Return a fixed-length numerical tuple for this workload."""
        values = [float(self.elapsed_time)]
        for transaction in self.transactions:
            values.extend(
                (
                    float(transaction.transaction_id),
                    float(transaction.burst_time),
                    float(transaction.priority),
                    float(transaction.remaining_time),
                    float(transaction.waiting_time),
                    float(transaction.completed),
                )
            )
        return tuple(values)

    def step(
        self, action: object
    ) -> tuple[tuple[float, ...], float, bool, bool, dict]:
        """Execute one action and return observation, reward, done flags, and info.

        Action ``i`` selects item ``i`` in the current ready queue. Invalid
        actions consume one decision attempt but do not change simulated time or
        any transaction state.
        """
        if self._terminated or self._truncated:
            return self.get_observation(), 0.0, self._terminated, self._truncated, {
                "already_done": True
            }

        ready = self._ready_transactions()
        self._decision_count += 1

        if isinstance(action, bool) or not isinstance(action, int) or not (0 <= action < len(ready)):
            if self._decision_count >= self.max_episode_steps:
                self._truncated = True
            return (
                self.get_observation(),
                -self.INVALID_ACTION_PENALTY,
                False,
                self._truncated,
                {"invalid_action": True, "ready_count": len(ready)},
            )

        selected = ready[action]
        waiting_processes = [transaction for transaction in ready if transaction is not selected]
        for transaction in waiting_processes:
            transaction.waiting_time += 1

        selected.remaining_time = max(0, selected.remaining_time - 1)
        self.elapsed_time += 1
        completed_now = selected.remaining_time == 0
        if completed_now:
            selected.completed = True

        reward = -self.STEP_PENALTY
        reward -= self.WAITING_PENALTY_PER_PROCESS * len(waiting_processes)
        if completed_now:
            reward += self.COMPLETION_REWARD

        self._terminated = all(transaction.completed for transaction in self.transactions)
        if not self._terminated and self._decision_count >= self.max_episode_steps:
            self._truncated = True

        info = {
            "transaction_id": selected.transaction_id,
            "completed_now": completed_now,
            "waiting_process_count": len(waiting_processes),
        }
        return self.get_observation(), reward, self._terminated, self._truncated, info

    def render(self) -> None:
        """Print the queue and current execution progress."""
        print(f"Elapsed simulated time: {self.elapsed_time}")
        print("Ready queue action mapping:")
        ready = self._ready_transactions()
        if not ready:
            print("  (empty)")
        for action, transaction in enumerate(ready):
            print(
                f"  action {action}: transaction {transaction.transaction_id} | "
                f"remaining {transaction.remaining_time}/{transaction.burst_time} | "
                f"priority {transaction.priority} | waiting {transaction.waiting_time}"
            )
        for transaction in self.transactions:
            if transaction.completed:
                print(f"  transaction {transaction.transaction_id}: COMPLETED")

