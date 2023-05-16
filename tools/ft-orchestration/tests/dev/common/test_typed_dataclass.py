"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for typed_dataclass decorator.
"""

from collections import deque
from dataclasses import dataclass, field
from typing import Deque, Optional, Union

import pytest
from src.common.typed_dataclass import bool_convertor, typed_dataclass


def test_iterable():
    """Test of list and tuple auto-conversion."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_list: list[int]
        my_tuple: tuple[int, ...]

    instance = _DC(my_list=["1", "2", "3"], my_tuple=["1", "2", "3"])

    assert instance.my_list == [1, 2, 3]
    assert instance.my_tuple == (1, 2, 3)


def test_dict():
    """Test of dict auto-conversion."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_dict: dict[str, int]

    instance = _DC(my_dict={1: "1", 42: "42"})
    assert instance.my_dict == {"1": 1, "42": 42}


def test_tuple():
    """Test of conversion of fixed-length heterogenous tuple."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_tuple: tuple[str, int, Optional[float]]

    instance = _DC(my_tuple=[1, "42", None])
    assert instance.my_tuple == ("1", 42, None)

    instance.my_tuple = (1, "42", "3.14")
    assert instance.my_tuple == ("1", 42, 3.14)


def test_convert_function():
    """Test of conversion with user-defined convert function."""

    def _fn(val):
        if isinstance(val, str):
            return list(map(int, val.split(",")))
        return val

    @typed_dataclass
    @dataclass
    class _DC:
        my_list: list[int] = field(metadata={"convert_func": _fn})

    instance = _DC(my_list="1,2,3,4")
    assert instance.my_list == [1, 2, 3, 4]


def test_complex_type():
    """Test of recursive type check/conversion."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_dict: dict[str, list[Optional[dict[int, str]]]]

    instance = _DC(my_dict={"first": [None, {1: "a", 11: "b"}, None, {3: "f", 6: "s"}]})
    assert instance.my_dict == {"first": [None, {1: "a", 11: "b"}, None, {3: "f", 6: "s"}]}

    instance.my_dict = {"first": (None, {"1": 4.2, "11": 5.21}, None, {3.2: 98, 6.6: 89})}
    assert instance.my_dict == {"first": [None, {1: "4.2", 11: "5.21"}, None, {3: "98", 6: "89"}]}


def test_bool_convertor():
    """Test of general purpose convert function for bool."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_bool: bool = field(metadata={"convert_func": bool_convertor})

    instance = _DC(my_bool="true")
    assert instance.my_bool is True

    instance.my_bool = "false"
    assert instance.my_bool is False

    instance.my_bool = 1
    assert instance.my_bool is True

    instance.my_bool = 0
    assert instance.my_bool is False


def test_not_dataclass():
    """Test raise of TypeError when class is not a dataclass."""

    with pytest.raises(TypeError):

        @typed_dataclass
        class _DC:
            # pylint: disable=too-few-public-methods
            my_int: int


def test_autoconvert_raise():
    """Test of scenarios when value cannot be automatically converted."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_list: Optional[list[int]] = None
        my_tuple: Optional[tuple[int, str, bool]] = None
        my_dict: Optional[dict[str, int]] = None
        my_int: int = 0

    instance = _DC()

    with pytest.raises(ValueError):
        instance.my_list = 1234

    with pytest.raises(ValueError):
        instance.my_tuple = (1, "a", True, 2)

    with pytest.raises(ValueError):
        instance.my_dict = ["a", "b", "c", "d"]

    with pytest.raises(ValueError):
        instance.my_int = "abcd"


def test_unknown_type():
    """Test raise of NotImplementedError when auto-conversion is used on unknown generic type."""

    @typed_dataclass
    @dataclass
    class _DC:
        unknown_type: Deque

    with pytest.raises(NotImplementedError):
        _DC(unknown_type=[1, 2])

    # solution for unknown type - define convert_func
    @typed_dataclass
    @dataclass
    class _DC:
        unknown_type: Deque = field(metadata={"convert_func": deque})

    instance = _DC(unknown_type=[1, 2])

    assert instance.unknown_type == deque([1, 2])


def test_union():
    """Test of typed_dataclass with union with more types. Auto-conversion will raise TypeError."""

    @typed_dataclass
    @dataclass
    class _DC:
        my_union: Union[str, int]

    instance = _DC(my_union="abcd")

    with pytest.raises(TypeError):
        # cannot choose str or int conversion
        instance.my_union = 0.123
