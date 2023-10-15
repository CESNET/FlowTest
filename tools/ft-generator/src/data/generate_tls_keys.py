#!/usr/bin/env python3
"""
Author(s): Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

A script to pregenerate TLS keys"""

import os
from pathlib import Path

NKEYS = 50

OUTPUT_FILE = Path("tlskeys.cpp")

TEMPLATE = """\
/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Pregenerated TLS keys
 *
 * This file has been generated by generate_tls_keys.py
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tlskeys.h"

namespace generator {

const std::vector<TlsKeyData> TLS_KEY_DATABASE {
//<<<INSERT KEYS HERE>>>
};

} // namespace generator
"""


def main():
    """
    The main function
    """
    src = ""
    for i in range(1, NKEYS + 1):
        print(f"{i}/{NKEYS}")
        common_name = f"ft-generator-{i}.com"
        os.system(
            "openssl req -x509 -newkey rsa:4096 -keyout /tmp/key.pem -out /tmp/cert.pem -sha256 -days 3650 -nodes "
            f'-subj "/C=XX/ST=StateName/L=CityName/O=CompanyName/OU=CompanySectionName/CN={common_name}" '
            "2>/dev/null"
        )
        os.system("openssl x509 -outform der </tmp/cert.pem >/tmp/cert.der")

        cert_bytes = Path("/tmp/cert.der").read_bytes()
        key_text = Path("/tmp/key.pem").read_text("utf8")

        src += "{\n"
        src += f'\t"{common_name}",\n'
        src += "\t{" + ", ".join(f"0x{b:02x}" for b in cert_bytes) + "},\n"
        src += "\t" + "".join(f'"{line}\\n"' for line in key_text.splitlines()) + ",\n"
        src += "},\n"

    src = TEMPLATE.replace("//<<<INSERT KEYS HERE>>>", src)
    if OUTPUT_FILE.exists():
        OUTPUT_FILE.rename(OUTPUT_FILE.with_suffix(".cpp.bak"))
    OUTPUT_FILE.write_text(src, encoding="utf-8")
    clang_format_file = Path(__file__).parent.joinpath("../../../../.clang-format").resolve()
    exit_code = os.system(f'clang-format -style="file:{clang_format_file}" -i {OUTPUT_FILE.as_posix()}')
    if exit_code != 0:
        print("WARNING: clang-format failed or not found, output will not be formatted")
    print(f"Output written to {OUTPUT_FILE} ({len(src)} B)")


if __name__ == "__main__":
    main()
