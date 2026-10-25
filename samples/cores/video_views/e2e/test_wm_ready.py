import importlib.util
import itertools
from pathlib import Path
import subprocess
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('views', Path(__file__).with_name('run.py'))
views = importlib.util.module_from_spec(spec)
spec.loader.exec_module(views)


class WindowManagerReady(unittest.TestCase):
    def setUp(self):
        self.x11 = Mock()
        self.x11.XOpenDisplay.return_value = 1
        self.x11.XDefaultRootWindow.return_value = 2
        self.x11.XCreateSimpleWindow.return_value = 3
        self.library = patch.object(views.ctypes, 'CDLL', return_value=self.x11)
        self.library.start()
        self.addCleanup(self.library.stop)
        self.environ = patch.dict(views.os.environ, {'DISPLAY': ':99'})
        self.environ.start()
        self.addCleanup(self.environ.stop)

    def test_waits_for_a_managed_client(self):
        pending = subprocess.CompletedProcess([], 0, 'WM_STATE: not found')
        managed = subprocess.CompletedProcess([], 0, 'window state: Normal')
        with patch.object(views.subprocess, 'run', side_effect=[pending, managed]) as run, \
                patch.object(views.time, 'sleep'), \
                patch.object(views.time, 'monotonic', side_effect=itertools.count()):
            self.assertEqual(views.wm_ready_probe(), 0)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args.args[0], ['xprop', '-id', '0x3', 'WM_STATE'])
        self.x11.XDestroyWindow.assert_called_once_with(1, 3)
        self.x11.XCloseDisplay.assert_called_once_with(1)

    def test_identity_property_is_not_management(self):
        identity = subprocess.CompletedProcess([], 0, 'window id # 0x10')
        with patch.object(views.subprocess, 'run', return_value=identity), \
                patch.object(views.time, 'sleep'), \
                patch.object(views.time, 'monotonic', side_effect=itertools.count()):
            self.assertEqual(views.wm_ready_probe(timeout=3), 1)
        self.x11.XDestroyWindow.assert_called_once_with(1, 3)

    def test_failed_connection(self):
        self.x11.XOpenDisplay.return_value = None
        self.assertEqual(views.wm_ready_probe(), 1)
        self.x11.XCreateSimpleWindow.assert_not_called()

    def test_probe_timeout_releases_client(self):
        with patch.object(views.subprocess, 'run', side_effect=subprocess.TimeoutExpired('xprop', 3)):
            with self.assertRaises(subprocess.TimeoutExpired):
                views.wm_ready_probe()
        self.x11.XDestroyWindow.assert_called_once_with(1, 3)
        self.x11.XCloseDisplay.assert_called_once_with(1)

    def test_start_requires_probe_success(self):
        wm = Mock()
        wm.poll.return_value = None
        identity = subprocess.CompletedProcess([], 0, 'window id # 0x10')
        ready = subprocess.CompletedProcess([], 0)
        with patch.object(views.subprocess, 'Popen', return_value=wm), \
                patch.object(views.subprocess, 'run', side_effect=[identity, ready]) as run:
            self.assertIs(views.wm_start({'DISPLAY': ':99'}), wm)
        self.assertEqual(run.call_args.args[0][-1], '--wm-ready')

    def test_unresponsive_manager_is_released(self):
        wm = Mock()
        wm.poll.return_value = None
        identity = subprocess.CompletedProcess([], 0, 'window id # 0x10')
        pending = subprocess.CompletedProcess([], 1)
        def command(args, **kwargs):
            return identity if '-root' in args else pending
        with patch.object(views.subprocess, 'Popen', return_value=wm), \
                patch.object(views.subprocess, 'run', side_effect=command), \
                patch.object(views.time, 'sleep'), \
                patch.object(views.time, 'monotonic', side_effect=itertools.count()):
            with self.assertRaises(RuntimeError):
                views.wm_start({'DISPLAY': ':99'})
        wm.terminate.assert_called_once()
        wm.wait.assert_called_once_with(5)


    def test_exited_manager_cannot_borrow_readiness(self):
        wm = Mock()
        wm.poll.side_effect = [None, 0, 0]
        identity = subprocess.CompletedProcess([], 0, 'window id # 0x10')
        ready = subprocess.CompletedProcess([], 0)
        with patch.object(views.subprocess, 'Popen', return_value=wm), \
                patch.object(views.subprocess, 'run', side_effect=[identity, ready]):
            with self.assertRaises(RuntimeError):
                views.wm_start({'DISPLAY': ':99'})
        wm.terminate.assert_called_once()
        wm.wait.assert_called_once_with(5)

    def test_exited_server_cannot_borrow_readiness(self):
        server = Mock()
        server.poll.return_value = 1
        with patch.object(views.subprocess, 'Popen', return_value=server), \
                patch.object(views, 'free_display', return_value=99), \
                patch.object(views, 'release_display') as release, \
                patch.object(views.os.path, 'exists', return_value=True), \
                patch.object(views.time, 'sleep'):
            with self.assertRaises(RuntimeError):
                views.xvfb_start({})
        release.assert_called_once_with(':99')
        server.terminate.assert_not_called()


    def test_server_without_socket_is_killed_and_released(self):
        server = Mock()
        server.poll.return_value = None
        server.wait.side_effect = [subprocess.TimeoutExpired('Xvfb', 5), None]
        with patch.object(views.subprocess, 'Popen', return_value=server), \
                patch.object(views, 'free_display', return_value=99), \
                patch.object(views, 'release_display') as release, \
                patch.object(views.os.path, 'exists', return_value=False), \
                patch.object(views.time, 'time', side_effect=itertools.count()), \
                patch.object(views.time, 'sleep'):
            with self.assertRaises(RuntimeError):
                views.xvfb_start({})
        server.terminate.assert_called_once()
        server.kill.assert_called_once()
        self.assertEqual(server.wait.call_count, 2)
        self.assertEqual(server.wait.call_args_list[0].args, (5,))
        self.assertEqual(server.wait.call_args_list[1].args, ())
        release.assert_called_once_with(':99')


if __name__ == '__main__':
    unittest.main()
