"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit tests for ftprofiler - core.py.
"""

import os
import sys
from unittest.mock import patch

import pytest
from ftprofiler import core
from ftprofiler.readers.nffile import Nffile


class TestCore:
    """Class for pytest core.py unit tests."""

    @staticmethod
    def test_version():
        """Test that main() with -V param can be executed"""
        with pytest.raises(SystemExit) as exception:
            with patch.object(sys, "argv", [None, "-V"]):
                core.main()
        assert isinstance(exception.value, SystemExit)
        assert exception.value.code == 0

    @staticmethod
    def test_main_init_readererror():
        """Test main() - error in nffile reader - Unable to locate or execute Nfdump binary"""
        with patch.object(sys, "argv", [None, "-o", "unittest_dummy", "nffile", "-M", "readerdir", "-R", "dummy"]):
            with patch("shutil.which", side_effect=[None]):
                assert core.main() == 1

    @staticmethod
    def test_main_init_profilereader(monkeypatch):
        """Test main() - mock iterator. New file dummy will be created with flow headers"""
        writer_outfile = "unittest_dummy"
        # remove file from any previous runs
        if os.path.exists(writer_outfile):
            os.remove(writer_outfile)
        monkeypatch.setattr(Nffile, "__init__", lambda *_: None)
        monkeypatch.setattr(Nffile, "__iter__", lambda *_: iter(""))

        with patch.object(sys, "argv", [None, "-o", writer_outfile, "nffile", "-M", "readerdir", "-R", "dummy"]):
            assert core.main() == 0
        assert os.path.exists(writer_outfile)
        os.remove(writer_outfile)
