# pylint: disable=protected-access
"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit tests for ftprofiler - writer.py.
"""
import os
from unittest.mock import patch, mock_open
import pytest
from test_cache import GenerateCache
from ftprofiler.flow import Flow
from ftprofiler.writer import ProfileWriter, OutputException


@pytest.fixture(scope="function", autouse=True)
def setup_teardown():
    """Setup and teardown for each function in test class. Remove output files with flows."""

    def rem():
        if os.path.exists(WriterHelper.DUMMY_OUTFILE):
            os.remove(WriterHelper.DUMMY_OUTFILE)
        if os.path.exists(WriterHelper.DUMMY_OUTFILE_GZ):
            os.remove(WriterHelper.DUMMY_OUTFILE_GZ)

    rem()
    yield
    rem()


class WriterHelper:
    """Helper for Writer class"""

    DUMMY_OUTFILE = "unittest_dummy"
    DUMMY_OUTFILE_GZ = f"{DUMMY_OUTFILE}.gz"

    @staticmethod
    def create_pw(name=DUMMY_OUTFILE, header=Flow.FLOW_CSV_FORMAT, compress=True) -> ProfileWriter:
        """Create profile writer instance"""
        profile_writer = ProfileWriter(name, header, compress)
        if compress:
            assert profile_writer._name == f"{name}.gz"
        else:
            assert profile_writer._name == name
        assert profile_writer._header == header
        assert profile_writer._compress == compress
        assert profile_writer._written_cnt == 0
        assert profile_writer._fd is None
        return profile_writer

    @staticmethod
    def assert_flow_was_written(called_methods: list, flow: str, compress: bool) -> bool:
        """Custom asserts for open() mock and flow file writing. Skip check for compressed file."""
        if compress:
            return True
        assert (
            len(called_methods) == 1
        ), f"Open() mock must have only one call (write()), but it has more!\nobj: {called_methods}"
        assert called_methods[0][1][0] == f"{flow}\n"
        return True


@pytest.mark.parametrize("compress", [True, False])
class TestWriter:
    """Class for pytest writer.py unit tests."""

    @staticmethod
    def test_init(compress):
        """Test __init__ method"""
        assert WriterHelper.create_pw(compress=compress)

    @staticmethod
    def test_enter(monkeypatch, compress):
        """Test __enter__ method - correct file is created"""
        monkeypatch.setattr(ProfileWriter, "__exit__", lambda *_: None)
        with WriterHelper.create_pw(compress=compress):
            pass
        if compress:
            assert os.path.exists(WriterHelper.DUMMY_OUTFILE_GZ)
            assert not os.path.exists(WriterHelper.DUMMY_OUTFILE)
        else:
            assert os.path.exists(WriterHelper.DUMMY_OUTFILE)
            assert not os.path.exists(WriterHelper.DUMMY_OUTFILE_GZ)

    @staticmethod
    def test_enter_exception(compress):
        """Test __enter__ method - raise error.
        Builtin open() is mocked - raise OSError, which raises OutputException"""
        with pytest.raises(OutputException):
            with patch("builtins.open") as mocked_open:
                mocked_open.side_effect = OSError()
                with WriterHelper.create_pw(compress=compress):
                    pass

    @staticmethod
    def test_exit(compress):
        """Test __exit__ method"""
        profile_writer = WriterHelper.create_pw(compress=compress)
        profile_writer.__enter__()
        assert profile_writer._fd is not None
        assert profile_writer._fd.closed is False

        profile_writer.__exit__(None, None, None)
        assert profile_writer._fd.closed is True

    @staticmethod
    def test_write(compress):
        """Test write() method - mock open(), test writing to the output file"""
        nrflows = 100
        written_cnt = 0
        with patch("builtins.open", mock_open()) as mocked_open:
            with WriterHelper.create_pw(compress=compress) as profile_writer:
                written_cnt += 1  # header
                assert profile_writer._written_cnt == written_cnt
                WriterHelper.assert_flow_was_written(mocked_open().method_calls, Flow.FLOW_CSV_FORMAT, compress)
                mocked_open.reset_mock()
                for _ in range(0, nrflows):
                    flow = GenerateCache.gen_unique_flow()
                    profile_writer.write(flow)
                    written_cnt += 1  # flows
                    WriterHelper.assert_flow_was_written(mocked_open().method_calls, str(flow), compress)
                    mocked_open.reset_mock()
                assert profile_writer._written_cnt == written_cnt

    @staticmethod
    def test_write_exception(compress):
        """Test write() method - raise an error.
        Builtin open() is mocked - raise OSError, which raises OutputException"""
        with pytest.raises(OutputException):
            with patch("builtins.open", mock_open()) as mocked_open:
                mocked_open.side_effect = OSError()  # must be here, does not work if moved below!
                with WriterHelper.create_pw(compress=compress) as profile_writer:
                    profile_writer.write("")
