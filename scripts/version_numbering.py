"""PlatformIO pre-script: writes <build dir>/generated/version.h.

Version scheme as in sw-001: the tag if HEAD is tagged, else <tag>_dev<hash>, else <hash>;
"_dirty" is added for staged, unstaged or untracked changes. The file lives in the build
directory, so building never dirties the working tree. Also records the environment name.
"""
from datetime import datetime, timezone
from pathlib import Path
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)


def git(repo_dir, *args):
    return subprocess.check_output(["git", "-C", str(repo_dir), *args], text=True,
                                   stderr=subprocess.DEVNULL).strip()


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


repo = Path(env["PROJECT_DIR"]).resolve()  # noqa: F821
out_dir = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
out_dir.mkdir(parents=True, exist_ok=True)
stamp = datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
(out_dir / "version.h").write_text(
    "#pragma once\n"
    f'#define VERSION_STRING "{version(repo)}"\n'
    f'#define BUILD_TIMESTAMP "{stamp}"\n'
    f'#define GTE_ENV_NAME "{env["PIOENV"]}"\n',  # noqa: F821
    encoding="utf-8",
)
env.Append(CPPPATH=[str(out_dir)])  # noqa: F821
