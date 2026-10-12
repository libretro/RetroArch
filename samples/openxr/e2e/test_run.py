"""run.py's case selection and GL lanes, without a GPU.

python3 -m unittest discover -s samples/openxr/e2e -p 'test_*.py'
"""

import copy
import importlib.util
import os
import unittest

# Under another name: run.py imports the views harness's run module.
_spec = importlib.util.spec_from_file_location(
    'openxr_e2e_run', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'run.py'))
run = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(run)


class GlLanes(unittest.TestCase):
    def setUp(self):
        self.before = copy.deepcopy(
            [{k: v for k, v in c.items() if k != 'check'} for c in run.CASES])
        self.gl_cases = run.gl_cases()

    def test_names(self):
        names = [c['name'] for c in self.gl_cases]
        self.assertEqual(len(set(names)), len(names))
        for c in self.gl_cases:
            self.assertTrue(c['name'].startswith(c['lane'] + '-'))

    def test_lanes(self):
        lanes = {c['lane']: (c['driver'], c['context'])
                 for c in self.gl_cases}
        self.assertEqual(lanes, {'glcore-egl': ('glcore', 'wayland'),
                                 'glcore-glx': ('glcore', 'x'),
                                 'gl-egl': ('gl', 'egl_x'),
                                 'gl-glx': ('gl', 'x')})
        self.assertTrue(all(c['driver'] in ('glcore', 'gl')
                            for c in self.gl_cases))

    def test_hardware(self):
        old = run.GL_BASE['glcore']
        run.GL_BASE['glcore'] = ['3ds-stereo', 'hw-3ds-stereo']
        try:
            gl_cases = run.gl_cases()
        finally:
            run.GL_BASE['glcore'] = old
        hw = [c for c in gl_cases if c.get('gl_hardware')]
        self.assertTrue(hw)
        # A core context is glcore's; gl takes a compatibility one.
        self.assertTrue(all(
            c['options']['video_views_test_hw']
            == ('gl' if c['driver'] == 'glcore' else 'gl_compat')
            for c in hw))
        self.assertEqual(set(c['driver'] for c in hw), {'glcore', 'gl'})
        self.assertTrue(all(c.get('options', {}).get('video_views_test_hw',
                                                     'off') == 'off'
                            for c in gl_cases if not c.get('gl_hardware')))

    def test_vulkan_left_out(self):
        self.assertTrue(all('VK_INSTANCE_LAYERS' not in c.get('env', {})
                            for c in self.gl_cases))
        self.assertTrue(all('baseline' not in c for c in self.gl_cases))

    def test_cases_unchanged(self):
        after = [{k: v for k, v in c.items() if k != 'check'}
                 for c in run.CASES]
        self.assertEqual(after, self.before)
        self.assertTrue(all('driver' not in c for c in run.CASES))

    def test_checks_kept(self):
        by_name = {c['name']: c for c in run.CASES + run.GL_EXTRA}
        for c in self.gl_cases:
            name = c['name'][len(c['lane']) + 1:]
            base = by_name[name]
            self.assertIs(c['check'], run.GL_CHECKS[c['driver']].get(
                name, base['check']))
            self.assertEqual(c['steps'], base['steps'])

    def test_lane_skips(self):
        names = [c['name'] for c in self.gl_cases]
        self.assertIn('glcore-glx-focus', names)
        self.assertNotIn('glcore-egl-focus', names)

    def test_gl_window(self):
        # gl's window shows the packed frame whole.
        for c in self.gl_cases:
            self.assertEqual(c.get('top_px'),
                             (400, 240) if c['driver'] == 'gl' else None)
        self.assertEqual(run.Result('.').top_px, (800, 240))

    def test_reinit_grace(self):
        # A Wayland lane's window comes back later after a reinit.
        for c in self.gl_cases:
            self.assertEqual(c.get('reinit_grace'),
                             1.5 if c['context'] == 'wayland' else None)
        self.assertEqual(run.Result('.').reinit_grace, 0.5)

    def test_smoke_lane(self):
        egl = set(c['name'][len('gl-egl-'):] for c in self.gl_cases
                  if c['lane'] == 'gl-egl')
        glx = set(c['name'][len('gl-glx-'):] for c in self.gl_cases
                  if c['lane'] == 'gl-glx')
        self.assertEqual(glx, set(run.GL_ONLY['gl-glx']))
        self.assertTrue(glx < egl)

    def test_extra_cases_gl_only(self):
        names = {c['name'] for c in run.CASES}
        self.assertTrue(all(c['name'] not in names for c in run.GL_EXTRA))
        self.assertEqual(len(run.CASES), 139)


class Select(unittest.TestCase):
    def test_unknown_refused(self):
        cases, error = run.select(['3ds-stereo', 'no-such-case'], False,
                                  False)
        self.assertIsNone(cases)
        self.assertIn('no-such-case', error)
        cases, error = run.select(['3ds-stereo'], True, False)
        self.assertIsNone(cases)
        self.assertIn('3ds-stereo', error)

    def test_named(self):
        cases, error = run.select(['glcore-glx-3ds-stereo'], True, False)
        self.assertIsNone(error)
        self.assertEqual([c['name'] for c in cases],
                         ['glcore-glx-3ds-stereo'])
        cases, error = run.select([], False, False)
        self.assertEqual(len(cases), len(run.CASES))

    def test_gl_validate_refused(self):
        cases, error = run.select([], True, True)
        self.assertIsNone(cases)
        self.assertIn('Vulkan', error)


class Capture(unittest.TestCase):
    class Res:
        def __init__(self, events):
            self.events = events

    def test_failed_read(self):
        res = self.Res([{'ev': 'capture_failed', 'sc': 3, 'status': 36054,
                         'error': 0}])
        self.assertTrue(run.capture_errors(res, {}))

    def test_binding(self):
        res = self.Res([{'ev': 'session', 'graphics': 'egl'}])
        self.assertTrue(run.capture_errors(
            res, {'lane': 'glcore-glx', 'context': 'x'}))
        self.assertFalse(run.capture_errors(
            res, {'lane': 'glcore-egl', 'context': 'wayland'}))
        self.assertFalse(run.capture_errors(res, {}))


if __name__ == '__main__':
    unittest.main()
