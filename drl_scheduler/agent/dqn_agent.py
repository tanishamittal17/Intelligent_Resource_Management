"""A small Deep Q-Network agent for ``SchedulingEnv``.

Actions are positions in the environment's current ready queue. Since that
queue removes completed transactions, valid action positions are recalculated
from the completion flags in each observation.
"""

from collections import deque
import random
from typing import NamedTuple, Optional, Sequence

import torch
from torch import nn
from torch.nn import functional as F


class Transition(NamedTuple):
    """One experience replay item."""

    state: torch.Tensor
    action: int
    reward: float
    next_state: torch.Tensor
    done: bool


class ReplayBuffer:
    """A fixed-capacity memory that samples past transitions at random."""

    def __init__(self, capacity: int, seed: Optional[int] = None) -> None:
        if isinstance(capacity, bool) or not isinstance(capacity, int) or capacity <= 0:
            raise ValueError("capacity must be a positive integer")
        self._items = deque(maxlen=capacity)
        self._random = random.Random(seed)

    def push(
        self,
        state: Sequence[float] | torch.Tensor,
        action: int,
        reward: float,
        next_state: Sequence[float] | torch.Tensor,
        done: bool,
    ) -> None:
        """Store a transition, copying observations so later edits cannot affect it."""
        self._items.append(
            Transition(
                torch.as_tensor(state, dtype=torch.float32).detach().cpu().clone(),
                int(action),
                float(reward),
                torch.as_tensor(next_state, dtype=torch.float32).detach().cpu().clone(),
                bool(done),
            )
        )

    def sample(self, batch_size: int) -> list[Transition]:
        """Return a random batch, or raise ValueError if the batch is too large."""
        if batch_size <= 0 or batch_size > len(self._items):
            raise ValueError("batch_size must be positive and no larger than the buffer")
        return self._random.sample(list(self._items), batch_size)

    def __len__(self) -> int:
        return len(self._items)


class QNetwork(nn.Module):
    """Small fully connected network mapping observations to action Q-values."""

    def __init__(self, observation_size: int, action_count: int, hidden_size: int = 64) -> None:
        super().__init__()
        if observation_size <= 0 or action_count <= 0 or hidden_size <= 0:
            raise ValueError("network sizes must be positive")
        self.layers = nn.Sequential(
            nn.Linear(observation_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, hidden_size),
            nn.ReLU(),
            nn.Linear(hidden_size, action_count),
        )

    def forward(self, observations: torch.Tensor) -> torch.Tensor:
        return self.layers(observations)


class DQNAgent:
    """DQN learner with epsilon-greedy choice, replay, and a target network."""

    # SchedulingEnv observation layout: elapsed time, then six values per process.
    _PROCESS_FEATURE_COUNT = 6
    _COMPLETED_FEATURE_OFFSET = 5

    def __init__(
        self,
        observation_size: int,
        action_count: int,
        *,
        hidden_size: int = 64,
        learning_rate: float = 1e-3,
        gamma: float = 0.99,
        epsilon: float = 1.0,
        epsilon_min: float = 0.05,
        epsilon_decay: float = 0.995,
        batch_size: int = 32,
        replay_capacity: int = 10_000,
        target_update_interval: int = 100,
        seed: Optional[int] = None,
        device: Optional[str | torch.device] = None,
    ) -> None:
        if observation_size != 1 + self._PROCESS_FEATURE_COUNT * action_count:
            raise ValueError("observation_size must match 1 + 6 * action_count")
        if batch_size <= 0 or target_update_interval <= 0:
            raise ValueError("batch_size and target_update_interval must be positive")
        if not 0.0 <= gamma <= 1.0:
            raise ValueError("gamma must be between 0 and 1")
        if not 0.0 <= epsilon_min <= epsilon <= 1.0:
            raise ValueError("epsilon values must satisfy 0 <= epsilon_min <= epsilon <= 1")
        if not 0.0 < epsilon_decay <= 1.0:
            raise ValueError("epsilon_decay must be in (0, 1]")

        if seed is not None:
            torch.manual_seed(seed)
        self.device = torch.device(device or "cpu")
        self.observation_size = observation_size
        self.action_count = action_count
        self.gamma = gamma
        self.epsilon = epsilon
        self.epsilon_min = epsilon_min
        self.epsilon_decay = epsilon_decay
        self.batch_size = batch_size
        self.target_update_interval = target_update_interval
        self._random = random.Random(seed)

        self.policy_network = QNetwork(observation_size, action_count, hidden_size).to(self.device)
        self.target_network = QNetwork(observation_size, action_count, hidden_size).to(self.device)
        self.sync_target_network()
        self.target_network.eval()
        self.optimizer = torch.optim.Adam(self.policy_network.parameters(), lr=learning_rate)
        self.replay_buffer = ReplayBuffer(replay_capacity, seed=seed)
        self.training_steps = 0

    @classmethod
    def from_environment(cls, env, **kwargs) -> "DQNAgent":
        """Build an agent using the observation and workload sizes of an env."""
        return cls(
            observation_size=len(env.get_observation()),
            action_count=len(env.transactions),
            **kwargs,
        )

    def _observation_tensor(self, observation) -> torch.Tensor:
        tensor = torch.as_tensor(observation, dtype=torch.float32, device=self.device).reshape(-1)
        if tensor.numel() != self.observation_size:
            raise ValueError(
                f"expected {self.observation_size} observation values, got {tensor.numel()}"
            )
        return tensor

    def _ready_action_count(self, observation) -> int:
        """Count unfinished processes; their action positions are 0..count-1."""
        values = self._observation_tensor(observation)
        count = 0
        for process_index in range(self.action_count):
            completed_index = (
                1
                + process_index * self._PROCESS_FEATURE_COUNT
                + self._COMPLETED_FEATURE_OFFSET
            )
            if values[completed_index].item() < 0.5:
                count += 1
        return count

    def select_action(
        self,
        observation,
        available_actions: Optional[Sequence[int]] = None,
        *,
        explore: bool = True,
    ) -> int:
        """Choose a valid ready-queue position using epsilon-greedy selection.

        Optional ``available_actions`` further restricts the ready queue. Invalid
        supplied indices are ignored. A ValueError is raised if no action is
        currently eligible, such as after the episode has terminated.
        """
        state = self._observation_tensor(observation)
        ready_count = self._ready_action_count(state)
        valid_actions = list(range(ready_count))
        if available_actions is not None:
            valid_set = set(valid_actions)
            valid_actions = [
                action
                for action in available_actions
                if isinstance(action, int)
                and not isinstance(action, bool)
                and action in valid_set
            ]
        if not valid_actions:
            raise ValueError("there are no eligible actions in the current observation")

        if explore and self._random.random() < self.epsilon:
            return self._random.choice(valid_actions)

        with torch.no_grad():
            q_values = self.policy_network(state.unsqueeze(0))[0]
            masked_values = torch.full_like(q_values, float("-inf"))
            masked_values[valid_actions] = q_values[valid_actions]
            return int(torch.argmax(masked_values).item())

    def remember(
        self,
        state,
        action: int,
        reward: float,
        next_state,
        terminated: bool,
        truncated: bool = False,
    ) -> None:
        """Save an experience; both termination and truncation end bootstrapping."""
        if isinstance(action, bool) or not isinstance(action, int):
            raise TypeError("action must be an integer")
        if not 0 <= action < self.action_count:
            raise ValueError("action is outside the network action range")
        state_tensor = self._observation_tensor(state).detach().cpu()
        next_tensor = self._observation_tensor(next_state).detach().cpu()
        self.replay_buffer.push(
            state_tensor,
            action,
            reward,
            next_tensor,
            bool(terminated or truncated),
        )

    def _compute_bellman_targets(
        self, rewards: torch.Tensor, next_states: torch.Tensor, dones: torch.Tensor
    ) -> torch.Tensor:
        """Compute r + gamma * max(Q_target), masking terminal transitions."""
        with torch.no_grad():
            next_values = torch.zeros_like(rewards, device=self.device)
            active_rows = torch.nonzero(~dones.bool(), as_tuple=False).flatten()
            if active_rows.numel() > 0:
                active_states = next_states[active_rows].to(self.device)
                q_values = self.target_network(active_states)
                for batch_row, state in enumerate(active_states):
                    ready_count = self._ready_action_count(state)
                    if ready_count > 0:
                        next_values[active_rows[batch_row]] = q_values[
                            batch_row, :ready_count
                        ].max()
            return rewards.to(self.device) + self.gamma * next_values

    def train_step(self) -> Optional[float]:
        """Train from one replay batch; return None until enough items exist."""
        if len(self.replay_buffer) < self.batch_size:
            return None

        transitions = self.replay_buffer.sample(self.batch_size)
        states = torch.stack([item.state for item in transitions]).to(self.device)
        actions = torch.tensor(
            [item.action for item in transitions], dtype=torch.long, device=self.device
        )
        rewards = torch.tensor(
            [item.reward for item in transitions], dtype=torch.float32, device=self.device
        )
        next_states = torch.stack([item.next_state for item in transitions]).to(self.device)
        dones = torch.tensor(
            [item.done for item in transitions], dtype=torch.bool, device=self.device
        )

        predicted = self.policy_network(states).gather(1, actions.unsqueeze(1)).squeeze(1)
        targets = self._compute_bellman_targets(rewards, next_states, dones)
        loss = F.smooth_l1_loss(predicted, targets)

        self.optimizer.zero_grad()
        loss.backward()
        nn.utils.clip_grad_norm_(self.policy_network.parameters(), max_norm=1.0)
        self.optimizer.step()

        self.training_steps += 1
        self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)
        if self.training_steps % self.target_update_interval == 0:
            self.sync_target_network()
        return float(loss.item())

    def sync_target_network(self) -> None:
        """Copy the latest policy weights to the target network."""
        self.target_network.load_state_dict(self.policy_network.state_dict())
