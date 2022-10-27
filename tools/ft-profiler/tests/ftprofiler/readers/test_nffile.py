# pylint: disable=protected-access
"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit tests for ftprofiler/reader - nffile.py.
"""
import subprocess
import argparse
from unittest.mock import patch, MagicMock
import pytest
from hypothesis import given, settings, HealthCheck
import hypothesis.strategies as st
from test_cache import GenerateCache
from ftprofiler.flow import Flow
from ftprofiler.readers.nffile import Nffile, InputException


class NffileHelper:
    """Helper class for nffile"""

    MOCKED_READ = 0
    TMP_FLOW = None

    @staticmethod
    def mock_argparse_cmd(count: int = 0) -> (argparse.Namespace, list):
        """Mock argparse instance and cmd variable to check"""
        mock_argparse = MagicMock(spec=argparse.Namespace)
        mock_argparse.multidir = "dummy1"
        mock_argparse.read = "dummy2"
        mock_argparse.count = count
        mock_argparse.router = "dummy3"
        mock_argparse.ifcid = "dummy4"
        fake_path = "dummypath"
        cmd = [
            fake_path,
            "-qN",
            "-M",
            mock_argparse.multidir,
            "-R",
            mock_argparse.read,
            "-o",
            Nffile.NFDUMP_FORMAT,
            "-6",
            f"not proto ARP and router ip {mock_argparse.router} and in if {mock_argparse.ifcid}",
        ]
        if count > 0:
            cmd[-1:-1] = ["-c", str(mock_argparse.count)]  # add to last but one position
        return mock_argparse, cmd

    @staticmethod
    def mock_nffile():
        """Mock Nffile instance"""
        mocked_argparse, mocked_cmd = NffileHelper.mock_argparse_cmd()
        with patch("shutil.which", side_effect=[mocked_cmd[0]]):
            return Nffile(mocked_argparse)

    @staticmethod
    def flow() -> bytes:
        """Generate flow bytes used by Nffile __next__ to mock nfdump response"""
        flow = GenerateCache.gen_unique_flow()
        NffileHelper.TMP_FLOW = flow
        if flow.swap:
            f_str = (
                f"{flow.start_time},"
                f"{flow.end_time - flow.start_time},"
                f"{flow.key[4]},"
                f"{flow.key[1]},"
                f"{flow.key[0]},"
                f"{flow.key[3]},"
                f"{flow.key[2]},"
                f"{flow.packets},"
                f"{flow.bytes}"
            )
        else:
            f_str = (
                f"{flow.start_time},"
                f"{flow.end_time - flow.start_time},"
                f"{flow.key[4]},"
                f"{flow.key[0]},"
                f"{flow.key[1]},"
                f"{flow.key[2]},"
                f"{flow.key[3]},"
                f"{flow.packets},"
                f"{flow.bytes}"
            )
        return f_str.encode()


class TestNffile:
    """Class for pytest nffile.py unit tests."""

    @staticmethod
    @given(st.integers())
    @settings(suppress_health_check=[HealthCheck.function_scoped_fixture])
    def test_init(count):
        """Test __init__() - success"""
        mocked_argparse, mocked_cmd = NffileHelper.mock_argparse_cmd(count)
        with patch("shutil.which", side_effect=[mocked_cmd[0]]):
            nffile = Nffile(mocked_argparse)
        assert nffile._cmd == mocked_cmd
        assert nffile._process is None
        assert nffile._zero_time is None

    @staticmethod
    def test_init_exception():
        """Test __init__() - exception is raised because Nfdump binary cannot be found."""
        mocked_argparse, _ = NffileHelper.mock_argparse_cmd()
        with pytest.raises(InputException):
            with patch("shutil.which", side_effect=[None]):
                Nffile(mocked_argparse)

    @staticmethod
    def test_iter_first_call():
        """Test __iter__() when called first time (_process attribute is None)"""
        nffile = NffileHelper.mock_nffile()
        with patch("ftprofiler.readers.nffile.Nffile.terminate") as mocked_term:
            with patch("ftprofiler.readers.nffile.Nffile._start_nfdump_process") as mocked_start:
                assert iter(nffile) == nffile
                mocked_term.assert_not_called()
                mocked_start.assert_called_once()

    @staticmethod
    def test_iter_second_call():
        """Test __iter__() when called second time (_process attribute is not None)"""
        nffile = NffileHelper.mock_nffile()
        with patch("ftprofiler.readers.nffile.Nffile.terminate") as mocked_term:
            with patch("ftprofiler.readers.nffile.Nffile._start_nfdump_process") as mocked_start:
                nffile._process = True
                assert iter(nffile) == nffile
                mocked_term.assert_called_once()
                mocked_start.assert_called_once()

    @staticmethod
    def test_next():
        """Test __next__() - success"""
        nffile = NffileHelper.mock_nffile()
        with patch("subprocess.Popen") as mocked_popen:
            mocked_popen.stdout = MagicMock(spec=subprocess.Popen.stdout)
            mocked_popen.stdout.readline = lambda *_: NffileHelper.flow()
        nffile._process = mocked_popen
        res = next(nffile)
        flow = NffileHelper.TMP_FLOW
        assert isinstance(res, Flow)
        assert res.swap == flow.swap
        assert res.start_time == 0
        assert res.end_time == (flow.end_time - flow.start_time) * 1000
        assert res.key == flow.key
        assert res.packets == flow.packets
        assert res.bytes == flow.bytes

    @staticmethod
    def test_next_empty():
        """Test __next__() - subprocess readline is empty"""
        nffile = NffileHelper.mock_nffile()
        with pytest.raises(StopIteration):
            with patch("subprocess.Popen") as mocked_popen:
                mocked_popen.stdout = MagicMock(spec=subprocess.Popen.stdout)
                mocked_popen.stdout.readline = lambda *_: []
                nffile._process = mocked_popen
                next(nffile)

    @staticmethod
    def test_next_exception_when_not_initialized():
        """Test __next__() - exception is raised when Nffile is not
        initialized (calling iter() first to set _process attribute)"""
        nffile = NffileHelper.mock_nffile()
        with pytest.raises(InputException):
            next(nffile)

    @staticmethod
    @pytest.mark.parametrize("poll_retcode", [0, 1])
    def test_next_continue_when_retcode_is_none(poll_retcode):
        """Test __next__() with stdout.readline() returning empty line.
        At first call, ret_code from process poll is None -> continue with next line.
        At second call, ret_code is not None, which produces exception."""

        def poll():
            NffileHelper.MOCKED_READ += 1
            if NffileHelper.MOCKED_READ == 1:
                return None
            return poll_retcode

        NffileHelper.MOCKED_READ = 0
        nffile = NffileHelper.mock_nffile()
        with pytest.raises(StopIteration):
            with patch("subprocess.Popen") as mocked_popen:
                mocked_popen.stdout = MagicMock(spec=subprocess.Popen.stdout)
                mocked_popen.stdout.readline = lambda *_: ""
                mocked_popen.poll = lambda *_: poll()
                nffile._process = mocked_popen
                next(nffile)

    @staticmethod
    def test_next_continue_when_no_stderr():
        """Test __next__() - first call of stdout.readline() returns invalid data and raises ValueError exception,
        but since stderr is empty, exception is suppressed and __next__() continues with next line read.
        Second line read returns empty string, which stops iteration."""

        def read_stdout():
            """Return different values for subprocess readline() iterations.
            Similar to mock side_effect"""
            NffileHelper.MOCKED_READ += 1
            if NffileHelper.MOCKED_READ == 1:
                return bytes(1)
            return ""

        NffileHelper.MOCKED_READ = 0
        nffile = NffileHelper.mock_nffile()
        with pytest.raises(StopIteration):
            with patch("subprocess.Popen") as mocked_popen:
                mocked_popen.stdout = MagicMock(spec=subprocess.Popen.stdout)
                mocked_popen.stderr = MagicMock(spec=subprocess.Popen.stderr)
                mocked_popen.stdout.readline = lambda *_: read_stdout()
                mocked_popen.stderr.readline = lambda *_: ""
                nffile._process = mocked_popen
                next(nffile)

    @staticmethod
    def test_next_exception_when_stderr():
        """Test __next__() - stdout.readline() returns invalid data and tries to raise ValueError exception.
        Since stderr is not empty, exception is raised."""
        nffile = NffileHelper.mock_nffile()
        with pytest.raises(InputException):
            with patch("subprocess.Popen") as mocked_popen:
                mocked_popen.stdout = MagicMock(spec=subprocess.Popen.stdout)
                mocked_popen.stdout.readline = lambda *_: bytes(1)
                mocked_popen.stderr = MagicMock(spec=subprocess.Popen.stderr)
                mocked_popen.stderr.readline = lambda *_: bytes(1)
                nffile._process = mocked_popen
                next(nffile)

    @staticmethod
    def test_start():
        """Test _start_nfdump_process() - success.
        We actually call Popen(), so we need to return our mocked new_popen instead of some new default, where we cannot
        change returncode class variable."""
        nffile = NffileHelper.mock_nffile()
        new_popen = MagicMock(spec=subprocess.Popen)
        new_popen.returncode = 0
        with patch("subprocess.Popen", side_effect=[new_popen]) as mocked_popen:
            nffile._process = mocked_popen
            nffile._start_nfdump_process()
        mocked_popen.assert_called_once()

    @staticmethod
    def test_start_exception():
        """Test _start_nfdump_process() - exception is raised when subprocess.Popen ends with return code 1."""
        nffile = NffileHelper.mock_nffile()
        new_popen = MagicMock(spec=subprocess.Popen)
        new_popen.returncode = 1
        with pytest.raises(InputException):
            with patch("subprocess.Popen", side_effect=[new_popen]) as mocked_popen:
                new_popen.stderr = MagicMock(spec=subprocess.Popen.stderr)
                new_popen.stderr.readline = lambda *_: bytes(1)
                nffile._process = mocked_popen
                nffile._start_nfdump_process()

    @staticmethod
    def test_terminate():
        """Test terminate() - success"""
        nffile = NffileHelper.mock_nffile()
        nffile._zero_time = 1000
        with patch("subprocess.Popen") as mocked_popen:
            mocked_kill = MagicMock(spec=subprocess.Popen.kill)
            mocked_popen.kill = mocked_kill
            nffile._process = mocked_popen
            nffile.terminate()
            mocked_kill.assert_not_called()
            assert nffile._process is None
            assert nffile._zero_time is None

    @staticmethod
    def test_terminate_forced():
        """Test terminate() - peaceful termination has timeout, so process is forcefully killed via kill()"""
        nffile = NffileHelper.mock_nffile()
        nffile._zero_time = 1000
        with patch("subprocess.Popen") as mocked_popen:
            mocked_popen.wait = MagicMock(spec=subprocess.Popen.wait, side_effect=TimeoutError())
            mocked_kill = MagicMock(spec=subprocess.Popen.kill)
            mocked_popen.kill = mocked_kill
            nffile._process = mocked_popen
            nffile.terminate()
            mocked_kill.assert_called_once()
            assert nffile._process is None
            assert nffile._zero_time is None
