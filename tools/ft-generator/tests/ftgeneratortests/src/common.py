"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains common module functions.
"""


def get_prob_from_str(text: str) -> float:
    """Function converts probability from string to float representation.

    Parameters
    ----------
    text : str
        Probability in string format.("30%" or "0.3")

    Returns
    -------
    float
        Converted probability, ranging from 0 to 1.

    Raises
    ------
    ValueError
        Raised when probability cannot be converted to float or if its out of range(0-100).
    """

    text = str(text)
    percentage = False

    if text[-1] == "%":
        percentage = True
        text = text[:-1]

    try:
        out = float(text)
    except ValueError as exc:
        raise ValueError("common::get_prob_from_str(): Unable to convert probability to float.") from exc

    if percentage:
        out /= 100
    if out < 0 or out > 1:
        raise ValueError("common::get_prob_from_str(): Probability in configuration is out of range.(0-100)")

    return out
