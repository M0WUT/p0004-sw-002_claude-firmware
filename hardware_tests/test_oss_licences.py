"""OSS Licences Service (issue #55): every Instrument reports the open source
components linked into its firmware, each with its licence and a link to the
authoritative licence text, plus the firmware version and where its source is.

Rows are read through the generic Status() path (the test Application's
print_status scenario prints every Service's Status Values on the UART). Expected
versions come from the pinned packages on this PC, not from the firmware, and the
expected licences from the packages' own licence files (read when writing this
test), so a wrong row in the firmware fails here.
"""

import json
import re
import subprocess
from pathlib import Path

from conftest import ENV, REPO, TEST_ENV, build_cached

SERVICE = "OSS_LICENCES"
PACKAGES = Path.home() / ".platformio/packages"
FRAMEWORK = PACKAGES / "framework-arduinoststm32"
SOURCE_URL = "https://github.com/M0WUT/p0004-sw-002_claude-firmware"


def spdx(expr: str, page: str = "") -> str:
    """A licence value: the SPDX expression and a link to its spdx.org page."""
    page = page or expr
    return f"{expr} (https://spdx.org/licenses/{page}.html)"


MIT = spdx("MIT")
LGPL = spdx("LGPL-2.1-or-later")
BSD3 = spdx("BSD-3-Clause")
APACHE = spdx("Apache-2.0")
NEWLIB = (
    "LicenseRef-newlib (https://sourceware.org/git/"
    "?p=newlib-cygwin.git;a=blob_plain;f=COPYING.NEWLIB)"
)
GCC = (
    "GPL-3.0-or-later WITH GCC-exception-3.1 "
    "(https://spdx.org/licenses/GPL-3.0-or-later.html "
    "https://spdx.org/licenses/GCC-exception-3.1.html)"
)
NADLER_HEAP = (
    "LicenseRef-Nadler-heap (https://github.com/stm32duino/STM32FreeRTOS/"
    "blob/10.3.2/portable/MemMang/heap_useNewlib_ST.c)"
)
# SPDX ids used in the list (each checked on spdx.org when writing this test).
# Anything else must be a LicenseRef- with its own link.
KNOWN_SPDX = {
    "MIT",
    "LGPL-2.1-or-later",
    "BSD-3-Clause",
    "Apache-2.0",
    "GPL-3.0-or-later",
    "GCC-exception-3.1",
}


def read(path: Path) -> str:
    return path.read_text(errors="replace")


def define(path: Path, name: str) -> int:
    m = re.search(rf"#define\s+{name}\s+\(?\s*(0x[0-9A-Fa-f]+|\d+)U?\s*\)?", read(path))
    assert m, f"{name} not found in {path}"
    return int(m.group(1), 0)


def core_version() -> str:
    h = FRAMEWORK / "libraries/SrcWrapper/inc/stm32_def.h"
    parts = (define(h, f"STM32_CORE_VERSION_{p}") for p in ("MAJOR", "MINOR", "PATCH"))
    return ".".join(str(p) for p in parts)


def hal_version() -> str:
    md = read(FRAMEWORK / "system/Drivers/STM32YYxx_HAL_Driver_version.md")
    m = re.search(r"STM32H7: (\d+\.\d+\.\d+)", md)
    assert m, "STM32H7 HAL version not listed"
    return m.group(1)


def cmsis_device_version() -> str:
    h = FRAMEWORK / "system/Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h7xx.h"
    parts = (
        define(h, f"__STM32H7xx_CMSIS_DEVICE_VERSION_{p}")
        for p in ("MAIN", "SUB1", "SUB2")
    )
    return ".".join(str(p) for p in parts)


def cmsis_core_version() -> str:
    h = PACKAGES / "framework-cmsis/CMSIS/Core/Include/cmsis_version.h"
    return (
        f"{define(h, '__CM_CMSIS_VERSION_MAIN')}.{define(h, '__CM_CMSIS_VERSION_SUB')}"
    )


def freertos_version() -> str:
    task_h = next(
        (REPO / ".pio/libdeps").glob(
            "*/STM32duino FreeRTOS/src/FreeRTOS/Source/include/task.h"
        )
    )
    m = re.search(r'tskKERNEL_VERSION_NUMBER\s+"V([\d.]+)"', read(task_h))
    assert m, "FreeRTOS kernel version not found"
    return m.group(1)


def tinyusb_version() -> str:
    return json.loads(read(REPO / "lib/tinyusb/library.json"))["version"]


def vrekrer_version() -> str:
    props = next(
        (REPO / ".pio/libdeps").glob("*/Vrekrer SCPI parser/library.properties")
    )
    m = re.search(r"^version=(\S+)$", read(props), re.M)
    assert m, "Vrekrer version not found"
    return m.group(1)


def newlib_version() -> str:
    h = PACKAGES / "toolchain-gccarmnoneeabi/arm-none-eabi/include/_newlib_version.h"
    m = re.search(r'_NEWLIB_VERSION "([\d.]+)"', read(h))
    assert m, "newlib version not found"
    return m.group(1)


def gcc_version() -> str:
    gcc = PACKAGES / "toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc"
    out = subprocess.run([str(gcc), "-dumpfullversion"], capture_output=True, text=True)
    return out.stdout.strip()


def expected_rows(version: str) -> dict[str, tuple[str, str]]:
    """Core's rows, in order: id -> (label, value)."""
    core = core_version()
    return {
        "firmware": (f"GTE Core firmware {version}", MIT),
        "source": ("Source code", SOURCE_URL),
        "arduinoCore": (f"STM32duino Arduino core {core}", LGPL),
        "wire": (f"STM32duino Wire library (core {core})", LGPL),
        "genericVariant": (f"STM32duino generic H743V variant (core {core})", BSD3),
        "stm32h7Hal": (f"STM32H7 HAL driver {hal_version()}", BSD3),
        "cmsisDevice": (f"ST CMSIS STM32H7 device {cmsis_device_version()}", APACHE),
        "cmsisCore": (f"Arm CMSIS-Core {cmsis_core_version()}", APACHE),
        "freertos": (f"FreeRTOS kernel {freertos_version()}", MIT),
        "freertosHeap": ("FreeRTOS newlib heap wrapper (Dave Nadler)", NADLER_HEAP),
        "tinyusb": (f"TinyUSB {tinyusb_version()}", MIT),
        "vrekrerScpi": (f"Vrekrer SCPI parser {vrekrer_version()} (d5354a0)", MIT),
        "newlib": (f"newlib-nano {newlib_version()}", NEWLIB),
        "gccRuntime": (f"GCC runtime and libstdc++ {gcc_version()}", GCC),
    }


# What each part of a build becomes in the list. A build folder that isn't here (a
# new library, say) fails test_every_linked_component_listed until it's added here
# and given a row in the firmware.
BUILD_PARTS = {
    "FrameworkArduino": {"arduinoCore"},
    "SrcWrapper": {"arduinoCore", "stm32h7Hal", "cmsisDevice"},
    "FrameworkArduinoVariant": {"genericVariant"},
    "Wire": {"wire"},
    # the kernel, and the newlib malloc wrapper heap.c pulls in
    "STM32duino FreeRTOS": {"freertos", "freertosHeap"},
    "tinyusb": {"tinyusb"},
    "Vrekrer SCPI parser": {"vrekrerScpi"},
    # Ours (MIT, the firmware row)
    "src": {"firmware"},
    "drivers": {"firmware"},
    "helpers": {"firmware"},
    "services": {"firmware"},
    "hardware_tests": {"firmware"},
}
# Always linked: CMSIS-Core headers, the C library and the compiler runtime; and the
# source row.
ALWAYS = {"cmsisCore", "newlib", "gccRuntime", "source"}
NOT_CODE = {"generated"}  # build-time outputs, not libraries


def built_parts(env: str) -> set[str]:
    """The framework parts and libraries PlatformIO compiled for env."""
    bdir = REPO / ".pio/build" / env
    parts = set()
    for d in bdir.iterdir():
        if not d.is_dir():
            continue
        if re.fullmatch(r"lib[0-9a-f]{3}", d.name):
            parts |= {lib.name for lib in d.iterdir() if lib.is_dir()}
        else:
            parts.add(d.name)
    return parts - NOT_CODE


def rows_on_uart(u) -> dict[str, tuple[str, str]]:
    """OSS_LICENCES rows printed by print_status: id -> (label, value), in order. A
    long value arrives as a "Status value" line and then "Status more" pieces."""
    labels: dict[str, str] = {}
    rows: dict[str, str] = {}
    line = re.compile(rf"\tStatus (label|value|more) {SERVICE} (\w+): (.*)$")
    for _, m in u.lines_matching(line.pattern):
        kind, i, text = m.groups()
        if kind == "label":
            labels[i] = text
        elif kind == "value":
            rows[i] = text
        else:
            rows[i] += text
    return {i: (labels.get(i, ""), v) for i, v in rows.items()}


def print_status(run_scenario):
    u = run_scenario("print_status")
    u.wait_for(r"\tStatus done$", 10)
    return u


def test_service_registered(booted, run_scenario):
    """Core firmware (no Application) and the test Application both start
    OSS_LICENCES."""
    started = rf"\[{SERVICE}\]\s*\tINFO\t\tInitialising$"
    booted.wait_for(started, 10)
    u = run_scenario("normal")
    u.wait_for(started, 10)


def test_firmware_and_source(run_scenario):
    """The first two rows: the firmware's version (MIT) and where its source is."""
    fw = build_cached(TEST_ENV)
    rows = rows_on_uart(print_status(run_scenario))
    assert list(rows)[:2] == ["firmware", "source"]
    assert rows["firmware"] == (f"GTE Core firmware {fw.version}", MIT)
    assert rows["source"] == ("Source code", SOURCE_URL)


def test_core_rows(run_scenario):
    """Every Core row, with the version of the package actually linked and its
    licence, in order."""
    fw = build_cached(TEST_ENV)
    rows = rows_on_uart(print_status(run_scenario))
    expected = expected_rows(fw.version)
    assert list(rows)[: len(expected)] == list(expected)
    for i, row in expected.items():
        assert rows[i] == row, f"row {i}"


def test_every_linked_component_listed(run_scenario):
    """Every framework part and library PlatformIO builds has a row, and every row is
    something that's built. Checked for the Core firmware and the test Application."""
    parts = {}
    for env in (ENV, TEST_ENV):
        build_cached(env)
        parts[env] = built_parts(env)
        unknown = parts[env] - set(BUILD_PARTS)
        assert not unknown, f"built for {env} but not acknowledged: {sorted(unknown)}"
    wanted = set().union(*(BUILD_PARTS[p] for p in parts[TEST_ENV])) | ALWAYS
    rows = rows_on_uart(print_status(run_scenario))
    core_rows = set(rows) - {"testLibrary"}  # the Application's own row
    assert core_rows == wanted


def test_licence_values(run_scenario):
    """Every value except the source URL is "<licence> (<links>)": each SPDX id is a
    known one with a link to its spdx.org page, anything else is a LicenseRef- with its
    own link. Where a package declares its licence, the row agrees."""
    rows = rows_on_uart(print_status(run_scenario))
    for i, (_, value) in rows.items():
        if i == "source":
            continue
        m = re.fullmatch(r"(.+?) \((https://\S+(?: https://\S+)*)\)", value)
        assert m, f"{i}: {value!r} is not '<licence> (<links>)'"
        ids = [t for t in m.group(1).split() if t not in ("WITH", "AND", "OR")]
        links = m.group(2).split()
        if ids[0].startswith("LicenseRef-"):
            assert len(ids) == 1 and len(links) == 1, f"{i}: {value!r}"
            continue
        assert set(ids) <= KNOWN_SPDX, f"{i}: unknown SPDX id in {ids}"
        assert links == [f"https://spdx.org/licenses/{x}.html" for x in ids], i

    # What the packages declare themselves
    cmsis = json.loads(read(PACKAGES / "framework-cmsis/package.json"))["license"]
    assert rows["cmsisCore"][1].startswith(cmsis + " ")
    licence_md = read(FRAMEWORK / "License.md")
    assert re.search(r"\[LGPLv2\.1\].*\n\n\* `cores/arduino/", licence_md)
    assert rows["arduinoCore"][1] == LGPL
    wire = read(FRAMEWORK / "libraries/Wire/src/Wire.cpp")
    assert "version 2.1 of the License, or (at your option) any later" in wire
    assert rows["wire"][1] == LGPL
    task_h = next(
        (REPO / ".pio/libdeps").glob(
            "*/STM32duino FreeRTOS/src/FreeRTOS/Source/include/task.h"
        )
    )
    assert "Permission is hereby granted, free of charge" in read(task_h)  # MIT
    assert rows["freertos"][1] == MIT


def test_application_acknowledgements(run_scenario):
    """The test Application's own library row (added through Acknowledge()) comes
    after Core's rows."""
    rows = rows_on_uart(print_status(run_scenario))
    ids = list(rows)
    assert ids[-1] == "testLibrary"
    assert ids.index("testLibrary") > ids.index("gccRuntime")
    assert rows["testLibrary"] == ("GTE test library 1.0", MIT)
