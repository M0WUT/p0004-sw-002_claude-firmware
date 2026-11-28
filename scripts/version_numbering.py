"""PlatformIO pre-script: writes <build dir>/generated/version.h.

Version scheme as in sw-001: the tag if HEAD is tagged, else <tag>_dev<hash>, else
<hash>;
"_dirty" is added for staged, unstaged or untracked changes. The file lives in the build
directory, so building never dirties the working tree. Also records the environment
name.
"""

import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:  # SCons provides Import() when PlatformIO runs this script

    def Import(*names: str) -> None: ...


Import("env")
env: Any = globals()["env"]  # put there by Import("env")


def git(repo_dir, *args):
    return subprocess.check_output(
        ["git", "-C", str(repo_dir), *args], text=True, stderr=subprocess.DEVNULL
    ).strip()


def version(repo_dir):
    commit_hash = git(repo_dir, "rev-parse", "--short", "HEAD")
    try:
        tag = git(repo_dir, "describe", "--tags", "--abbrev=0")
    except subprocess.CalledProcessError:
        tag = None
    if tag is None:
        v = commit_hash
    else:
        n = int(git(repo_dir, "rev-list", "--count", f"{tag}..HEAD"))
        v = f"{tag}_dev{commit_hash}" if n > 0 else tag
    if git(repo_dir, "status", "--porcelain"):
        v += "_dirty"
    return v


def version_bcd(repo_dir):
    """The latest tag's major.minor as USB bcdDevice, two BCD digits each (v1.2 ->
    0x0102, v1.12 -> 0x0112, which lsusb shows as 1.02 and 1.12), 0 with no tag
    (Release USB identity, issue #38)."""
    try:
        tag = git(repo_dir, "describe", "--tags", "--abbrev=0")
    except subprocess.CalledProcessError:
        return 0
    nums = [int(p) for p in tag.lstrip("v").split(".")[:2] if p.isdigit()]
    major, minor = (nums + [0, 0])[:2]
    major, minor = min(major, 99), min(minor, 99)
    return (major // 10 << 12) | (major % 10 << 8) | (minor // 10 << 4) | minor % 10


repo = Path(env["PROJECT_DIR"]).resolve()
out_dir = Path(env.subst("$BUILD_DIR")) / "generated"
out_dir.mkdir(parents=True, exist_ok=True)
stamp = datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
(out_dir / "version.h").write_text(
    "#pragma once\n"
    f'#define VERSION_STRING "{version(repo)}"\n'
    f'#define BUILD_TIMESTAMP "{stamp}"\n'
    f'#define GTE_ENV_NAME "{env["PIOENV"]}"\n'
    f"#define GTE_CORE_VERSION_BCD 0x{version_bcd(repo):04X}\n",
    encoding="utf-8",
)
# Included as "generated/version.h" (Google style: includes name their directory).
env.Append(CPPPATH=[str(out_dir.parent)])
