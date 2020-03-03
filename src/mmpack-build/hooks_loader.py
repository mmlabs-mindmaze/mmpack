# @mindmaze_header@
"""
Helper to load available hooks.

To create a new hook, just create a python file source prefixed with
"hook_" in the current source folder and implement a class called
MMPackBuildHook that derived from BaseHook of base_hook.py.
"""

import importlib
import pkgutil
from os.path import dirname

from . common import dprint


MMPACK_BUILD_HOOKS = []


def init_mmpack_build_hooks(srcname: str, host_archdist: str,
                            description: str) -> None:
    """
    To be called in the early stages of package creation mmpack-build, it
    populates the list of build hooks plugins and initializes the hooks
    with some important metadata that is constant all across the build.

    Args:
        srcname: name of source package being built
        host_archdist: architecture/distribution of the host, ie which
            arch/dist the package is being built for
        description: description of the source package

    Returns:
        None
    """
    global MMPACK_BUILD_HOOKS  # pylint: disable=global-statement

    for _, name, _ in pkgutil.iter_modules([dirname(__file__)]):
        if not name.startswith('hook_'):
            continue

        try:
            module = importlib.import_module('mmpack_build.' + name)

            # Instantiate hook and add it to the list
            hook = module.MMPackBuildHook(srcname, host_archdist, description)
            MMPACK_BUILD_HOOKS.append(hook)
            dprint('hook plugin loaded: {}'.format(hook.__module__))

        except AttributeError:
            raise ImportError('invalid mmpack-build hook module: {}'
                              .format(name))
