"""PlatformIO pre-script: use STM32duino's generic H743V variant files in place.

The board variant (variants/p0004_pcb_001_v1) holds only what is board-specific.
The generic files it builds on (PeripheralPins.c, PinNamesVar.h, variant_generic.*,
ldscript.ld) come straight from the framework package, which is pinned by the
platform version in platformio.ini, so there are no copies to keep in step:
- the framework's variant folder goes on the include path, so the board variant's
  one-line wrappers can #include its sources and headers;
- the board uses the framework's linker script for that folder.
"""

from pathlib import Path
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:  # SCons provides Import() when PlatformIO runs this script

    def Import(*names: str) -> None: ...


Import("env")
env: Any = globals()["env"]  # put there by Import("env")

GENERIC = "STM32H7xx/H742V(G-I)(H-T)_H743V(G-I)(H-T)_H750VBT_H753VI(H-T)"

framework = Path(env.PioPlatform().get_package_dir("framework-arduinoststm32"))
generic = framework / "variants" / GENERIC
if not (generic / "variant_generic.cpp").is_file():
    raise SystemExit(f"framework_variant.py: no generic variant at {generic}")


# Compiler and linker flags pass through a shell, so escape the folder name's
# parentheses, as the framework's own build script does for its variant folder.
def shell_safe(path: Path) -> str:
    return str(path).replace("(", r"\(").replace(")", r"\)")


env.Append(CPPPATH=[shell_safe(generic)])
# Setting build.ldscript stops the framework adding the board variant's own
# (absent) linker script; LDSCRIPT_PATH is what the link actually uses.
env.BoardConfig().update("build.ldscript", str(generic / "ldscript.ld"))
env.Replace(LDSCRIPT_PATH=shell_safe(generic / "ldscript.ld"))
