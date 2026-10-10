import unittest

import torch

from drl_scheduler.agent import DQNAgent, QNetwork, ReplayBuffer
from drl_scheduler.env import SchedulingEnv


class DQNAgentTests(unittest.TestCase):
    def test_network_returns_one_q_value_per_action(self):
        network = QNetwork(observation_size=19, action_count=3, hidden_size=8)
        output = network(torch.zeros((4, 19)))

        self.assertEqual(tuple(output.shape), (4, 3))

    def test_agent_sizes_itself_from_environment(self):
        env = SchedulingEnv()
        agent = DQNAgent.from_environment(env, seed=7)

        self.assertEqual(agent.observation_size, len(env.get_observation()))
        self.assertEqual(agent.action_count, len(env.transactions))

    def test_action_selection_returns_only_ready_queue_positions(self):
        env = SchedulingEnv()
        agent = DQNAgent.from_environment(env, epsilon=1.0, seed=3)
        # Complete the last original item so the ready-queue action indices shrink.
        observation, _, _, _, _ = env.step(2)

        action = agent.select_action(observation)

        self.assertIn(action, (0, 1))
        self.assertIn(agent.select_action(observation, available_actions=[99, 1]), (1,))

    def test_greedy_action_selection_is_valid_and_no_actions_is_safe(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 1, "priority": 1}]
        )
        agent = DQNAgent.from_environment(env, epsilon=0.0, epsilon_min=0.0, seed=1)
        self.assertEqual(agent.select_action(env.get_observation(), explore=False), 0)
        terminal_observation, _, terminated, _, _ = env.step(0)

        self.assertTrue(terminated)
        with self.assertRaises(ValueError):
            agent.select_action(terminal_observation)

    def test_replay_buffer_keeps_capacity_and_samples(self):
        memory = ReplayBuffer(capacity=2, seed=2)
        for value in range(3):
            memory.push([float(value)], value, 1.0, [float(value + 1)], False)

        sample = memory.sample(2)

        self.assertEqual(len(memory), 2)
        self.assertEqual(len(sample), 2)
        self.assertEqual({item.action for item in sample}, {1, 2})

    def test_training_step_updates_from_a_small_batch(self):
        env = SchedulingEnv()
        agent = DQNAgent.from_environment(
            env, batch_size=2, replay_capacity=8, epsilon=0.8, seed=4
        )
        first = env.get_observation()
        next_observation, reward, _, _, _ = env.step(0)
        agent.remember(first, 0, reward, next_observation, False)
        agent.remember(next_observation, 0, reward, next_observation, False)

        loss = agent.train_step()

        self.assertIsInstance(loss, float)
        self.assertTrue(torch.isfinite(torch.tensor(loss)).item())
        self.assertEqual(agent.training_steps, 1)
        self.assertLess(agent.epsilon, 0.8)

    def test_terminal_transition_does_not_bootstrap(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 1, "priority": 1}]
        )
        agent = DQNAgent.from_environment(env, batch_size=1, seed=5)
        state = env.get_observation()
        next_state, _, terminated, _, _ = env.step(0)
        self.assertTrue(terminated)

        targets = agent._compute_bellman_targets(
            torch.tensor([2.5]),
            torch.tensor([next_state]),
            torch.tensor([True]),
        )
        agent.remember(state, 0, 2.5, next_state, terminated)
        loss = agent.train_step()

        self.assertEqual(targets.item(), 2.5)
        self.assertTrue(torch.isfinite(targets).all().item())
        self.assertIsInstance(loss, float)
        self.assertTrue(torch.isfinite(torch.tensor(loss)).item())

    def test_truncated_transition_is_treated_as_terminal(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 2, "priority": 1}],
            max_episode_steps=1,
        )
        agent = DQNAgent.from_environment(env, batch_size=1, seed=6)
        state = env.get_observation()
        next_state, reward, terminated, truncated, _ = env.step(0)
        self.assertFalse(terminated)
        self.assertTrue(truncated)
        agent.remember(state, 0, reward, next_state, terminated, truncated)

        self.assertTrue(agent.replay_buffer._items[-1].done)
        self.assertTrue(torch.isfinite(torch.tensor(agent.train_step())).item())


if __name__ == "__main__":
    unittest.main()
