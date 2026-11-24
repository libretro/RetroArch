"""The process may exit between a completion predicate and its liveness check."""
import unittest
from stream import wait_until


class Process:
    def __init__(self, status):
        self.returncode = status

    def poll(self):
        return self.returncode


class WaitTest(unittest.TestCase):
    def test_exit_after_predicate_is_valid_while_quitting(self):
        process = Process(None)

        def predicate():
            process.returncode = 0
            return False

        wait_until(predicate, process, allow_exit=True)
        self.assertEqual(process.returncode, 0)

    def test_exit_before_acceptance_is_failure(self):
        with self.assertRaisesRegex(AssertionError, 'frontend exited early'):
            wait_until(lambda: False, Process(0))

    def test_failed_quit_status_is_preserved_for_caller(self):
        process = Process(3)
        wait_until(lambda: False, process, allow_exit=True)
        self.assertEqual(process.returncode, 3)


if __name__ == '__main__':
    unittest.main()
