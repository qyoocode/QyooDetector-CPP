#!/usr/bin/env python3
"""The manifest recall gate must fail without hiding the rejection category."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

import test_generated_safety as runner


class RequiredDecodeContract(unittest.TestCase):
    def run_fixture(self, stdout, expected=1, require_decode=None, returncode=0):
        # Exercise the actual manifest reader/report writer/exit gate. Only the
        # external detector process is replaced with a controlled CLI response.
        here = Path(__file__).resolve()
        work = here.parents[1] / '.codex-work/generated-safety-contract'
        work.mkdir(parents=True, exist_ok=True)
        fixture = dict(file=str(here), expected_payload=expected,
                       sha256=hashlib.sha256(here.read_bytes()).hexdigest())
        if require_decode is not None:
            fixture['require_decode'] = require_decode
        manifest, report = work/'manifest.json', work/'report.json'
        manifest.write_text(json.dumps(dict(fixtures=[fixture])))
        args = ['runner', '--manifest', str(manifest), '--detector', str(here),
                '--work-dir', str(work), '--report', str(report)]
        response = subprocess.CompletedProcess([], returncode, stdout, '')
        with patch.object(sys, 'argv', args), patch.object(runner.subprocess, 'run', return_value=response), contextlib.redirect_stdout(io.StringIO()):
            status = runner.main()
        return status, json.loads(report.read_text())

    diagnostic = 'Diagnostics JSON = {"image_loaded":true}\n'

    def test_required_positive_rejection_fails_and_stays_rejected(self):
        status, report = self.run_fixture(self.diagnostic, require_decode=True)
        self.assertEqual(status, 1)
        self.assertEqual(report['counts']['rejected'], 1)
        self.assertEqual(report['required_decode_rejections'], 1)
        self.assertEqual(report['counts']['accepted_wrong'], 0)
        self.assertEqual(report['counts']['processing_error'], 0)

    def test_old_manifest_and_explicit_optional_rejections_still_pass(self):
        for required in (None, False):
            with self.subTest(required=required):
                status, report = self.run_fixture(self.diagnostic, require_decode=required)
                self.assertEqual(status, 0)
                self.assertEqual(report['counts']['rejected'], 1)

    def test_required_correct_decode_passes(self):
        status, report = self.run_fixture(self.diagnostic+'Qyoo value = 1\n', require_decode=True)
        self.assertEqual(status, 0)
        self.assertEqual(report['counts']['accepted_correct'], 1)

    def test_required_wrong_decode_remains_wrong_and_fails(self):
        status, report = self.run_fixture(self.diagnostic+'Qyoo value = 0\n', require_decode=True)
        self.assertEqual(status, 1)
        self.assertEqual(report['counts']['accepted_wrong'], 1)

    def test_required_processing_error_remains_error_and_fails(self):
        status, report = self.run_fixture('', require_decode=True, returncode=1)
        self.assertEqual(status, 1)
        self.assertEqual(report['counts']['processing_error'], 1)

    def test_negative_control_remains_correct_negative(self):
        status, report = self.run_fixture(self.diagnostic, expected=None)
        self.assertEqual(status, 0)
        self.assertEqual(report['counts']['correct_negative'], 1)


if __name__ == '__main__':
    unittest.main()
