"""Config storage (Build: config storage, issue #43): sw-001's EEPROM format,
migrations, the blank/corrupt prompts, Factory Reset and the last-reset record.

The tests run on the real EEPROM. Writes are kept to a few bytes: the firmware
writes only bytes that change, and pytest damages or resets the store through
"config actions" that the test Application applies before config loads. pytest
reads the firmware's RAM copy of the config area over SWD to check the format.
"""

import re
import time

import pytest

from conftest import (
    BOOT_RE,
    PROMPT_RE,
    SCENARIOS,
    TEST_ENV,
    TEST_RAM_ENV,
    TEST_V2_ENV,
    TEST_V3GAP_ENV,
    build_cached,
    hold_keys,
    openocd,
    read_config_image,
    reset_with,
)

pytestmark = pytest.mark.hardware

CORE, APP, AUX, OSS_LICENCES = 0, 13, 14, 16
KEY1, KEY4 = 1 << 0, 1 << 3


# --------------------------------------------- independent model of the format


def crc8(data: bytes) -> int:
    """sw-001's CRC8 (RobTillaart CRC8(7, 0, 0x55))."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0x55


def le16(b: bytes, at: int) -> int:
    return b[at] | (b[at + 1] << 8)


def parse(img: bytes) -> dict[int, tuple[int, int, bytes]]:
    """{service id: (base address, version, data)}, checking every CRC."""
    assert img[:3] == b"LID", f"no LID header: {img[:4].hex()}"
    n = img[3]
    end = 4 + 2 * n
    assert crc8(img[:end]) == img[end], "allocation table CRC"
    blocks = {}
    for sid in range(n):
        base = le16(img, 4 + 2 * sid)
        if base == 0xFFFF:
            continue
        total = le16(img, base)
        crc_at = base + total - 1
        data_at = base + 3
        assert crc8(img[base:crc_at]) == img[crc_at], f"block CRC, Service {sid}"
        blocks[sid] = (base, img[base + 2], img[data_at:crc_at])
    return blocks


def image(env: str = TEST_ENV) -> bytes:
    return read_config_image(build_cached(env))


def config_lines(u, service: str) -> list[str]:
    pattern = re.compile(rf"\[{service}\]\s*\tINFO\t\t  (.+)$")
    return [m.group(1) for _, line in list(u.lines) if (m := pattern.search(line))]


def wait_values_set(u):
    u.wait_for(r"\[APPLICATION_TEST\].*\tvalues set$", 5)


def wait_started(u):
    u.wait_for(r"starting the scheduler$", 5)


# ------------------------------------------------------------------- storage


def test_values_persist(run_scenario):
    """Values set through the generic path, including Log levels, survive a
    reset. Blocks span pages and start mid-page, exercising the driver."""
    u = run_scenario("set_values")
    wait_values_set(u)
    u = run_scenario("normal", config=None)
    wait_started(u)
    app = config_lines(u, "APPLICATION_TEST")
    assert "Count (count): 20" in app
    assert "Gain (gain): 3.0 dB" in app
    assert "Name (name): PERSIST" in app
    # TEST_AUX is now at WARNING, so its config printout is (correctly) hidden:
    # check its stored values instead.
    assert not config_lines(u, "TEST_AUX"), "TEST_AUX's Log level not restored"
    _, _, aux = parse(image())[AUX]
    assert aux[1] == 2, "TEST_AUX Log level WARNING"
    assert int.from_bytes(aux[2:6], "little") == 9, "rate"


def test_format_matches_sw001(run_scenario):
    """The stored bytes (re-read from the chip at boot) follow docs/eeprom.md,
    checked by an independent model."""
    u = run_scenario("set_values")
    wait_values_set(u)
    u = run_scenario("normal", config=None)
    wait_started(u)
    blocks = parse(image())
    assert {CORE, APP, AUX} <= set(blocks)
    _, version, data = blocks[APP]
    assert version == 1
    assert data[0] == 1, "base header format byte"
    assert data[1] == 1, "Log level INFO"
    assert int.from_bytes(data[2:6], "little") == 20, "count"
    _, _, aux = blocks[AUX]
    assert aux[1] == 2, "TEST_AUX Log level WARNING"
    assert int.from_bytes(aux[2:6], "little") == 9, "rate"


def test_block_grows_and_shifts(run_scenario):
    """A block that grows is reallocated by shifting; the others keep their data."""
    u = run_scenario("set_values")
    wait_values_set(u)
    before = parse(image())
    u = run_scenario("normal", env=TEST_V2_ENV, config=None)
    wait_started(u)
    after = parse(image(TEST_V2_ENV))
    assert len(after[AUX][2]) == len(before[AUX][2]) + 40, "TEST_AUX did not grow"
    if before[APP][0] > before[AUX][0]:
        assert after[APP][0] == before[APP][0] + 40, "APPLICATION_TEST not shifted"
    assert "Count (count): 20" in config_lines(u, "APPLICATION_TEST")
    assert int.from_bytes(after[AUX][2][2:6], "little") == 9, "TEST_AUX rate"


def test_eeprom_full_fatal(run_scenario, uart):
    """A config too big for the EEPROM is a Fatal Error naming the Service."""
    with pytest.raises(pytest.fail.Exception):
        run_scenario("eeprom_full")  # never reaches "Test scenario"
    uart.wait_for(r"\[ConfigStorage\]\s*\tERROR\t20\tConfig EEPROM full: TEST_BIG$", 2)


def test_stale_application_block_ignored(run_scenario):
    """A block for a ServiceId no longer registered stays, ignored."""
    u = run_scenario("set_values")
    wait_values_set(u)
    u = run_scenario("no_app", config=None)
    u.wait_for(r"No Application$", 5)
    wait_started(u)
    blocks = parse(image())
    assert APP in blocks and AUX in blocks, "stale blocks were removed"


# ----------------------------------------------------------------- migration


def test_migration_chain(run_scenario):
    """A version 1 block loads in version 2 firmware through the 1->2 hook."""
    u = run_scenario("set_values")
    wait_values_set(u)
    u = run_scenario("normal", env=TEST_V2_ENV, config=None)
    wait_started(u)
    app = config_lines(u, "APPLICATION_TEST")
    assert "Offset (offset): 40" in app, "1->2 hook sets offset = 2 x count"
    _, _, aux = parse(image(TEST_V2_ENV))[AUX]
    assert aux[6:14] == b"MIGRATED", "TEST_AUX 1->2 hook"
    assert parse(image(TEST_V2_ENV))[APP][1] == 2, "migrated block saved at v2"


def test_missing_migration_fatal(run_scenario, uart):
    """No 1->2 step: a Fatal Error naming the Service and the Soft Keys 1+4 fix."""
    u = run_scenario("normal")
    wait_started(u)
    with pytest.raises(pytest.fail.Exception):
        run_scenario("normal", env=TEST_V3GAP_ENV, config=None)
    uart.wait_for(
        r"\[APPLICATION_TEST\]\s*\tERROR\t21\tNo config migration \(hold Soft Keys "
        r"1\+4 for 5 s at power-on to Factory Reset\): APPLICATION_TEST v1->v2$",
        3,
    )


def test_downgrade_fatal(run_scenario, uart):
    """Version 2 data in version 1 firmware is a Fatal Error."""
    u = run_scenario("normal", env=TEST_V2_ENV)
    wait_started(u)
    with pytest.raises(pytest.fail.Exception):
        run_scenario("normal", config=None)
    uart.wait_for(
        r"\tERROR\t21\tNo config migration .*: \w+ stored v2, firmware v1$", 3
    )


# ------------------------------------------------------ blank and corrupt data


def prompt_scenario(run_scenario, uart, action: str) -> None:
    """Store values (set_values), then reset with the store damaged, into the
    prompt."""
    u = run_scenario("set_values")
    wait_values_set(u)
    uart.clear()
    reset_with(SCENARIOS["normal"], config_action=action)
    uart.wait_for(BOOT_RE, 5)
    uart.wait_for(PROMPT_RE, 3)


def test_blank_prompt(run_scenario, uart):
    """Blank EEPROM: boot stops, prompt repeats every 2 s, no Service, no reset."""
    prompt_scenario(run_scenario, uart, "blank")
    time.sleep(10)
    prompts = uart.lines_matching(
        r"\[ConfigStorage\]\s*\tERROR\t\tBlank EEPROM detected\. Initialise with "
        r"defaults\? Hold Soft Key 4 for 3 s to confirm\.$"
    )
    assert 5 <= len(prompts) <= 7, f"{len(prompts)} prompts in ~10 s"
    gaps = [b[0] - a[0] for a, b in zip(prompts, prompts[1:])]
    assert all(1.8 < g < 2.3 for g in gaps), gaps
    assert not uart.lines_matching(r"starting the scheduler")
    assert len(uart.lines_matching(BOOT_RE)) == 1, "the board reset"
    hold_keys(KEY4)  # leave the board initialised for the next test
    uart.wait_for(r"Soft Key 4 held for 3 s: re-initialising$", 6)
    hold_keys(0)


def test_corrupt_block_prompt(run_scenario, uart):
    """A Service's CRC failure names that Service and says corrupt."""
    prompt_scenario(run_scenario, uart, "corrupt_aux")
    uart.wait_for(
        r"\[ConfigStorage\]\s*\tERROR\t\tConfig for TEST_AUX corrupt \(CRC failed\)\. "
        r"Hold Soft Key 4 for 3 s to reset TEST_AUX to defaults\.$",
        2,
    )
    hold_keys(KEY4)
    uart.wait_for(r"Soft Key 4 held for 3 s: re-initialising$", 6)
    hold_keys(0)


def test_corrupt_table_prompt(run_scenario, uart):
    """An allocation table CRC failure names the table."""
    prompt_scenario(run_scenario, uart, "corrupt_table")
    uart.wait_for(r"\tConfig allocation table corrupt \(CRC failed\)\.", 2)
    hold_keys(KEY4)
    uart.wait_for(r"Soft Key 4 held for 3 s: re-initialising$", 6)
    hold_keys(0)


def test_hold_key4_reinitialises(run_scenario, uart):
    """Holding Soft Key 4 for 3 s re-initialises only the failed block; a short
    hold does nothing."""
    prompt_scenario(run_scenario, uart, "corrupt_aux")
    hold_keys(KEY4)
    time.sleep(1.5)
    hold_keys(0)
    time.sleep(2.5)
    assert not uart.lines_matching(r"re-initialising"), "a 1.5 s hold re-initialised"
    hold_keys(KEY4)
    uart.wait_for(r"Soft Key 4 held for 3 s: re-initialising$", 6)
    hold_keys(0)
    uart.wait_for(r"\[TEST_AUX\].*\tConfig reset to defaults$", 3)
    wait_started(uart)
    assert "Rate (rate): 5" in config_lines(uart, "TEST_AUX")
    assert "Count (count): 20" in config_lines(uart, "APPLICATION_TEST")


# ------------------------------------------------------------- Factory Reset


def test_factory_reset_keys_at_power_on(run_scenario, uart):
    """Soft Keys 1+4 held 5 s at boot: everything to defaults, stale blocks
    reclaimed, restart. A 3 s hold cancels."""
    u = run_scenario("set_values")
    wait_values_set(u)

    uart.clear()
    reset_with(SCENARIOS["normal"], keys=KEY1 | KEY4)
    uart.wait_for(r"keep holding for 5 s to Factory Reset$", 3)
    time.sleep(3)
    hold_keys(0)
    uart.wait_for(r"Factory Reset cancelled \(keys released\)$", 2)
    wait_started(uart)
    assert "Count (count): 20" in config_lines(uart, "APPLICATION_TEST")

    # Factory Reset with no Application registered: afterwards only Core's own
    # Services (CORE, OSS_LICENCES) have blocks, so the Application's and
    # TEST_AUX's blocks were reclaimed.
    uart.clear()
    reset_with(SCENARIOS["no_app"], keys=KEY1 | KEY4)
    uart.wait_for(r"Factory Reset done; restarting$", 8)
    hold_keys(0)
    uart.wait_for(r"(?s)Factory Reset done.*No Application", 5)
    uart.wait_for(r"(?s)Factory Reset done.*starting the scheduler", 5)
    blocks = parse(image())
    assert set(blocks) == {
        CORE,
        OSS_LICENCES,
    }, f"blocks after Factory Reset: {sorted(blocks)}"

    u = run_scenario("normal", config=None)
    wait_started(u)
    assert "Count (count): 10" in config_lines(u, "APPLICATION_TEST")


def test_factory_reset_before_config_load(run_scenario, uart):
    """Factory Reset still works when a migration Fatal Error would stop the boot."""
    u = run_scenario("normal")
    wait_started(u)
    with pytest.raises(pytest.fail.Exception):
        run_scenario("normal", env=TEST_V3GAP_ENV, config=None)
    uart.wait_for(r"\tERROR\t21\t", 3)
    uart.clear()
    reset_with(SCENARIOS["normal"], keys=KEY1 | KEY4)
    uart.wait_for(r"Factory Reset done; restarting$", 8)
    hold_keys(0)
    uart.wait_for(r"(?s)Factory Reset done.*starting the scheduler", 8)
    assert not uart.lines_matching(r"\tERROR\t21\t")


# ------------------------------------------- RAM-only build, MAC, last-reset record


def test_debug_eeprom_is_ram_only(run_scenario):
    """DEBUG_EEPROM: no prompt on a fresh board, values lost at every reset."""
    u = run_scenario("set_values", env=TEST_RAM_ENV, config=None)
    u.wait_for(r"DEBUG_EEPROM: config is in RAM only, lost at every reset$", 2)
    wait_values_set(u)
    u = run_scenario("normal", env=TEST_RAM_ENV, config=None)
    wait_started(u)
    assert "Count (count): 10" in config_lines(u, "APPLICATION_TEST")
    assert not u.lines_matching(PROMPT_RE)


def mac_line(u) -> str:
    m = u.wait_for(r"\[CORE\].*\tMAC ([0-9A-F:]{17})$", 5)
    return m.group(1)


def test_mac_untouched(run_scenario, uart):
    """The factory MAC reads the same before and after a Factory Reset."""
    u = run_scenario("normal")
    before = mac_line(u)
    uart.clear()
    reset_with(SCENARIOS["normal"], keys=KEY1 | KEY4)
    uart.wait_for(r"Factory Reset done; restarting$", 8)
    hold_keys(0)
    uart.wait_for(r"(?s)Factory Reset done.*GTE boot env=", 5)
    uart.trim_before_last(BOOT_RE)
    assert mac_line(uart) == before


def test_fatal_record_logged_next_boot(run_scenario):
    """The fatal loop's polled-I2C record: the last Fatal Error is logged once
    at the next boot."""
    u = run_scenario("fatal")
    u.wait_for(r"\tERROR\t1450\tTest fatal error: boom$", 5)
    time.sleep(0.5)
    expected = (
        r"\[CORE\]\s*\tINFO\t\tLast Fatal Error: 1450 Test fatal error: boom "
        r"\(APPLICATION_TEST\)$"
    )
    u = run_scenario("normal", config=None)
    assert u.lines_matching(expected), "record not logged"
    u = run_scenario("normal", config=None)
    assert not u.lines_matching(expected), "record logged twice"


def test_watchdog_record_and_warning(run_scenario):
    """After a watchdog reset: the Service is logged and a Warning raised."""
    u = run_scenario("hang")
    u.wait_for(r"Hanging on purpose$", 5)
    u.wait_for(r"(?s)Hanging on purpose.*GTE reset reason=watchdog\n", 8)
    u.wait_for(r"\[CORE\]\s*\tINFO\t\tLast reset: watchdog, Service TEST_AUX$", 3)
    u.wait_for(
        r"\[CORE\]\s*\tWARNING\t105\tRecovered from a watchdog reset: "
        r"Service TEST_AUX$",
        3,
    )


# ----------------------------------------------------------------- Soft Keys

GPIOE_MODER, GPIOE_PUPDR = 0x58021000, 0x5802100C
SOFT_KEY_PINS = (10, 12, 9, 14)  # Soft Keys 1-4: PE10, PE12, PE9, PE14


def test_soft_key_pins(run_scenario):
    """The Soft Key pins are inputs with pull-ups."""
    u = run_scenario("normal")
    wait_started(u)
    out = openocd(
        f'init; echo "M=[read_memory {GPIOE_MODER:#x} 32 1] '
        f'P=[read_memory {GPIOE_PUPDR:#x} 32 1]"; shutdown'
    )
    m = re.search(r"M=(\S+) P=(\S+)", out)
    assert m, out
    moder, pupdr = int(m.group(1), 0), int(m.group(2), 0)
    for pin in SOFT_KEY_PINS:
        assert (moder >> (2 * pin)) & 3 == 0, f"PE{pin} not an input"
        assert (pupdr >> (2 * pin)) & 3 == 1, f"PE{pin} has no pull-up"


@pytest.mark.manual
def test_real_soft_keys(run_scenario):
    """Manual: press Soft Keys 1-4 in turn when asked."""
    u = run_scenario("normal")
    wait_started(u)
    for key in range(1, 5):
        print(f"\n>>> Press and release Soft Key {key} (left to right)")
        u.wait_for(rf"\[SoftKeys\].*\tSoft Key {key} pressed$", 30)
        u.wait_for(rf"\[SoftKeys\].*\tSoft Key {key} released$", 30)
