"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Add type check and automatic conversion of fields to a dataclass.
"""

from dataclasses import dataclass, is_dataclass
from typing import Any, Union, get_args, get_origin

SUPPORTED_ITERABLES = (list, tuple, set)


def typed_dataclass(cls: dataclass):
    """Add type check and automatic conversion of fields to a dataclass. Preferably use as decorator.

    Typical usage is when the field values come from the command line - values are strings.
    Decorator adds a mechanism that checks whether the value is instance of desired type while each value
    assignment (setattr). Then try to automatically convert value or use user-defined convert function.

    Auto-converted could be simple types (str, int, float, etc.), generic iterable types (list, tuple, set)
    and generic dict. Also is implemented support for typing.Union. Union could be auto-converted if exactly
    1 type is not None (Union[None, int] == Optional[int]).

    The custom convert function has to be defined in dataclass field metadata as 'convert_func' dictionary item.

    E.g.
        @typed_dataclass
        @dataclass
        class _DC:
            one: ComplexUserType = field(metadata={"convert_func": _fn})  # field will be converted with _fn(x)
            two: Optional[int] = None                                     # field will be auto-converted to int
    """

    def __setattr__(self, __name: str, __value: Any) -> None:
        """Custom __setattr__ method. Method triggered when any dataclass field is set.
        It is used to check and convert value to desired type.

        Parameters
        ----------
        __name : str
            Name of set field.
        __value : Any
            Value to check and convert.

        Raises
        ------
        TypeError
            When cls is not dataclass.
        TypeError, ValueError or NotImplementedError
            When auto-conversion fails. Custom convert function has to be provided by user in that case.
        """

        def _isinstance(value: Any, arg_type: type) -> bool:
            """Check if value is instance of specified type.

            Works with generic types.
            Nested values are recursively checked.
            """

            origin = get_origin(arg_type)

            if origin:
                # Generic type, such as Dict[str, int] or Union[int, str].
                # Recursive check is required.
                types = get_args(arg_type)

                if origin is Union:
                    return any(_isinstance(value, t) for t in types)

                if origin in SUPPORTED_ITERABLES:
                    if origin == tuple and not _tuple_is_variable_len(types):
                        # tuple of fixed length, may be heterogenous
                        return (
                            isinstance(value, tuple)
                            and len(value) == len(types)
                            and all(_isinstance(x, types[i]) for i, x in enumerate(value))
                        )
                    return isinstance(value, origin) and all(_isinstance(x, types[0]) for x in value)

                if origin is dict:
                    return isinstance(value, dict) and all(
                        _isinstance(k, types[0]) and _isinstance(v, types[1]) for k, v in value.items()
                    )

                raise NotImplementedError

            # Standard type, such as int or str.
            return isinstance(value, arg_type)

        def _retype(value: Any, arg_type: type) -> Any:
            """Try to automatically convert value.

            Works with generic types. Recursive conversion is used.
            """

            # If value has the expected type, there is no need of further retyping.
            if _isinstance(value, arg_type):
                return value

            origin = get_origin(arg_type)

            if origin:
                # Generic type, such as Dict[str, int] or Union[int, str].
                types = get_args(arg_type)

                if origin is Union:
                    return _retype_union(value, types)

                if origin in SUPPORTED_ITERABLES:
                    return _retype_iterable(value, origin, types)

                if origin is dict:
                    return _retype_dict(value, types)

                raise NotImplementedError(
                    f"Field: {__name}, Automatic conversion of generic type {arg_type} is not implemented. "
                    "Please specify convert function in field metadata."
                )

            # Standard type, such as int or str.
            try:
                return arg_type(value)
            except ValueError as ex:
                raise ValueError(
                    f"Field: {__name}, Cannot convert value to type {arg_type}. "
                    "Please specify convert function in field metadata."
                ) from ex

        def _retype_union(value: Any, types: list) -> Any:
            """Handle Union generic type conversion.

            Automatically convert Optional[tp] or Union[tp, None].
            Other scenario raise TypeError.
            """

            valid_types = list(filter(lambda x: x is not type(None), types))

            if len(valid_types) == 1:
                return _retype(value, valid_types[0])

            raise TypeError(
                f"Field: {__name}, Cannot automatically convert to type Union{types}. "
                "Please specify convert function in field metadata."
            )

        def _retype_iterable(value: Any, origin: type, types: list) -> Any:
            """Handle generic iterable type conversion. List, set, tuple.

            Tuple may be variable-length or heterogenous fixed-length.
            """

            # value must be iterable before converting.
            if not isinstance(value, SUPPORTED_ITERABLES):
                raise ValueError(
                    f"Field: {__name}, Cannot automatically convert from {type(value)} to type {origin}. "
                    "Please specify convert function in field metadata."
                )

            if origin == tuple and not _tuple_is_variable_len(types):
                # tuple of fixed length, may be heterogenous
                if len(value) != len(types):
                    raise ValueError(
                        f"Field: {__name}, Cannot automatically convert fixed-length tuple. "
                        "Different length of type arguments and input iterable. "
                        "Please specify convert function in field metadata."
                    )
                return tuple(_retype(x, types[i]) for i, x in enumerate(value))

            # variable-length homogenous iterable
            return origin([_retype(x, types[0]) for x in value])

        def _retype_dict(value: Any, types: type) -> dict:
            """Handle generic dict type conversion."""

            # value must be dict before converting, items are recursively converted
            if not isinstance(value, dict):
                raise ValueError(
                    f"Field: {__name}, Cannot automatically convert from {type(value)} to type 'dict'. "
                    "Please specify convert function in field metadata."
                )

            return {_retype(k, types[0]): _retype(v, types[1]) for k, v in value.items()}

        def _tuple_is_variable_len(types: list) -> bool:
            """Check if tuple generic type is variable length (Ellipsis is used: 'tuple[tp, ...]')."""

            if Ellipsis in types:
                if len(types) == 2 and types[1] == Ellipsis and types[0] != Ellipsis:
                    return True
                raise TypeError(
                    f"Field: {__name}, Not valid generic tuple use. "
                    "Variable-length tuple must be in form 'tuple[some_type, ...]'."
                )
            return False

        field = self.__dataclass_fields__[__name]

        if field.metadata and "convert_func" in field.metadata:
            convert_func = field.metadata["convert_func"]
            try:
                if not _isinstance(__value, field.type):
                    __value = convert_func(__value)
            except NotImplementedError:
                # _isinstance method may not be able to check type, then use convert_func unconditionally
                __value = convert_func(__value)
        else:
            __value = _retype(__value, field.type)

        super(cls, self).__setattr__(__name, __value)

    if not is_dataclass(cls):
        raise TypeError("Class is not dataclass. Use 'typed_dataclass' only with dataclasses.")

    setattr(cls, "__setattr__", __setattr__)
    return cls


def bool_convertor(value: Any) -> bool:
    """General purpose convert function for bool fields.
    'true' and 'false' string values are allowed. When value is not a string, standard 'bool' function is used.
    """

    if isinstance(value, str):
        if value not in ["true", "false"]:
            raise ValueError("String representation of bool must be 'true' or 'false'.")
        return value == "true"

    try:
        return bool(value)
    except ValueError as ex:
        raise ValueError(
            f"Cannot convert value of type {type(value)} to bool. "
            "Use string 'true' or 'false' or value which can be converted by builtin 'bool' function."
        ) from ex
