"""
Author(s):  Matej Hulák <hulak@cesnet.cz>
            Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Pytest conftest file.
"""

from pathlib import Path
from typing import Union

from ftgeneratortests.src import Generator
from pytest import CollectReport, FixtureRequest, Parser, StashKey, fixture, hookimpl

phase_report_key = StashKey[dict[str, CollectReport]]()


def pytest_addoption(parser: Parser):
    """Pytest addoption function, used to parse command line arguments.

    Parameters
    ----------
    parser : Parser
        Parser for command line arguments and ini-file values.
    """

    parser.addoption(
        "--executable",
        action="store",
        default=None,
        help="Path to ft-generator executable. None if ft-generator installed on system.",
    )
    parser.addoption(
        "--profiles",
        action="store",
        default=Path(__file__).parent.joinpath("../profiles/profiles_all.csv").resolve().as_posix(),
        help="Path to file with flow profiles.",
    )
    parser.addoption(
        "--tmp",
        action="store",
        default="ft_generator_tests_tmp_dir",
        help="Path to tmp directory, will be created if non existent.",
    )
    parser.addoption(
        "--config",
        action="store",
        default=None,
        help="Path to custom ft-generator config to run tests.",
    )


@hookimpl(hookwrapper=True, tryfirst=True)
def pytest_runtest_makereport(item, call):  # pylint: disable=unused-argument
    """Allow access to test results in fixture teardown

    Taken from:
    https://docs.pytest.org/en/latest/example/simple.html#making-test-result-information-available-in-fixtures
    """

    # execute all other hooks to obtain the report object
    rep = yield

    # store test results for each phase of a call, which can
    # be "setup", "call", "teardown"
    res = rep.get_result()
    item.stash.setdefault(phase_report_key, {})[res.when] = res

    return rep


@fixture(name="exec_path")
def fixture_exec_path(request: Parser) -> Union[Path, None]:
    """Fixture parses command line argument 'executable'.

    Parameters
    ----------
    request : Parser
        Parser for command line arguments and ini-file values.

    Returns
    -------
    Path
        Path to ft_generator executable or None if no argument provided.
    """
    path = request.config.option.executable
    if not path:
        return None

    return Path(path).absolute()


@fixture(name="profiles")
def fixture_profiles(request: Parser) -> Path:
    """Fixture parses command line argument 'profiles'.

    Parameters
    ----------
    request : Parser
        Parser for command line arguments and ini-file values.

    Returns
    -------
    Path
        Path to profiles file.

    Raises
    ------
    FileNotFoundError
        Raises exception if file cannot be found.
    """

    path = Path(request.config.option.profiles)
    if not path.exists():
        raise FileNotFoundError(f"conftest: Argument 'profiles': File '{path}' cannot be found.")

    return path


@fixture(name="tmp")
def fixture_tmp(request: Parser) -> Path:
    """Fixture parses command line argument 'tmp'.

    Parameters
    ----------
    request : Parser
        Parser for command line arguments and ini-file values.

    Returns
    -------
    Path
        Path to temporary directory.
    """

    return Path(request.config.option.tmp)


@fixture(name="custom_config")
def fixture_custom_config(request: Parser) -> Union[Path, None]:
    """Fixture parses command line argument 'config'.

    Parameters
    ----------
    request : Parser
        Parser for command line arguments and ini-file values.

    Returns
    -------
    Union[Path, None]
        Path to config file. None if no argument provided.

    Raises
    ------
    FileNotFoundError
        Raises exception if file cannot be found.
    """

    path = request.config.option.config
    if not path:
        return None

    path = Path(path)
    if not path.exists():
        raise FileNotFoundError("conftest: Argument 'config': File with config cannot be found.")

    return path.absolute()


@fixture(name="ft_generator")
def fixture_ft_generator(exec_path: Union[Path, None], profiles: Path, tmp: Path, request: FixtureRequest) -> Generator:
    """Fixtures creates instance of Generator with configured paths.

    Parameters
    ----------
    exec_path : Union[Path, None]
        Path to generator executable. None if not provided.
    profiles : Path
        Path to profiles files.
    tmp : Path
        Path to temporary directory.

    Returns
    -------
    Generator
        Instance of Generator with configured paths.
    """
    if not tmp.exists():
        tmp.mkdir()
    test_dir = tmp / request.node.name  # request.node.name is the name of the test function

    generator = Generator(generator_file=exec_path, profiles_file=profiles, tmp_dir=test_dir)

    yield generator

    # Remove the test output files unless the test failed
    report = request.node.stash[phase_report_key]
    if not ("call" in report and report["call"].failed):
        generator.clear()
        test_dir.rmdir()
