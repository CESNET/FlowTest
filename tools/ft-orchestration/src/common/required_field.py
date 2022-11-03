"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Custom required field for dataclasses. Used as workaround for mixing required and optional fields without ordering.
Useful for dataclasses inheritance. In python 3.10, similar behavior can be achieved with kw_only=True.
"""

from dataclasses import field


def required_field(**kwargs):
    """Create required field in dataclass.

    Example usage::

        @dataclass
        class A:
            a: Optional[int] = None
            b: Optional[str] = None

        @dataclass
        class C(A):
            x: int = required_field(repr=True, compare=False)
            y: int = -1

    Returns
    -------
    dataclasses.Field
        Field with defined default_factory which raises error when activated.

    Raises
    ------
    TypeError
        When value of field is not present.
    ValueError
        If field default value is specified.
    """

    # pylint: disable=too-few-public-methods
    class ErrorFactory:
        """Factory used to raise error when called. If field has a value, init is never called."""

        def __init__(self):
            raise TypeError(f"Field '{field_instance.name}' is required.")

    if "default" in kwargs:
        raise ValueError("Required field cannot have a default value.")

    kwargs["default_factory"] = ErrorFactory
    field_instance = field(**kwargs)
    return field_instance
