import unittest

from drl_scheduler.env import SchedulingEnv


class SchedulingEnvTests(unittest.TestCase):
    def test_reset_initializes_default_workload(self):
        env = SchedulingEnv()
        observation = env.reset()

        self.assertEqual(len(env.transactions), 3)
        self.assertEqual(env.elapsed_time, 0)
        self.assertEqual(observation, env.get_observation())
        self.assertTrue(all(not transaction.completed for transaction in env.transactions))
        self.assertTrue(
            all(transaction.remaining_time == transaction.burst_time for transaction in env.transactions)
        )

    def test_observation_shape_and_values_are_numeric(self):
        env = SchedulingEnv()
        observation = env.get_observation()

        self.assertEqual(len(observation), 1 + 6 * len(env.transactions))
        self.assertTrue(all(isinstance(value, (int, float)) for value in observation))

    def test_valid_action_runs_one_unit(self):
        env = SchedulingEnv()
        transaction = env.transactions[0]
        old_remaining = transaction.remaining_time

        observation, reward, terminated, truncated, info = env.step(0)

        self.assertEqual(transaction.remaining_time, old_remaining - 1)
        self.assertEqual(env.elapsed_time, 1)
        self.assertGreater(len(observation), 0)
        self.assertFalse(terminated)
        self.assertFalse(truncated)
        self.assertEqual(info["transaction_id"], transaction.transaction_id)
        self.assertLess(reward, 0)

    def test_invalid_action_does_not_change_transaction_state(self):
        env = SchedulingEnv()
        before = env.get_observation()

        observation, reward, terminated, truncated, info = env.step(99)

        self.assertEqual(observation, before)
        self.assertEqual(env.elapsed_time, 0)
        self.assertEqual(reward, -env.INVALID_ACTION_PENALTY)
        self.assertFalse(terminated)
        self.assertFalse(truncated)
        self.assertTrue(info["invalid_action"])

    def test_remaining_time_decreases_consistently(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 3, "priority": 1}]
        )

        env.step(0)
        self.assertEqual(env.transactions[0].remaining_time, 2)
        env.step(0)
        self.assertEqual(env.transactions[0].remaining_time, 1)

    def test_other_ready_transactions_accrue_waiting_time(self):
        env = SchedulingEnv()

        env.step(0)

        self.assertEqual(env.transactions[0].waiting_time, 0)
        self.assertEqual(env.transactions[1].waiting_time, 1)
        self.assertEqual(env.transactions[2].waiting_time, 1)

    def test_completion_terminates_episode(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 1, "priority": 1}]
        )

        observation, reward, terminated, truncated, info = env.step(0)

        self.assertTrue(env.transactions[0].completed)
        self.assertEqual(env.transactions[0].remaining_time, 0)
        self.assertTrue(terminated)
        self.assertFalse(truncated)
        self.assertTrue(info["completed_now"])
        self.assertGreater(reward, 0)
        self.assertEqual(len(observation), 7)

    def test_completion_reward_exceeds_noncompletion_reward(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 2, "priority": 1}]
        )

        _, step_reward, _, _, _ = env.step(0)
        _, completion_reward, _, _, _ = env.step(0)

        self.assertGreater(completion_reward, step_reward)

    def test_reset_is_deterministic(self):
        env = SchedulingEnv()
        initial_observation = env.reset()
        env.step(1)

        reset_observation = env.reset()

        self.assertEqual(reset_observation, initial_observation)
        self.assertEqual(env.elapsed_time, 0)
        self.assertEqual([item.waiting_time for item in env.transactions], [0, 0, 0])

    def test_time_limit_sets_truncated_without_terminating(self):
        env = SchedulingEnv(
            workload=[{"transaction_id": 1, "burst_time": 2, "priority": 1}],
            max_episode_steps=1,
        )

        _, _, terminated, truncated, _ = env.step(0)

        self.assertFalse(terminated)
        self.assertTrue(truncated)
        self.assertFalse(env.transactions[0].completed)


if __name__ == "__main__":
    unittest.main()
