# @mindmaze_header@
"""
Base class abstracting system package manager
"""

from typing import List


class SysPkgManager:
    """
    Base class of interaction with system package manager
    """

    def find_sharedlib_sysdep(self, soname: str, symbols: List[str]) -> str:
        """
        Parses the installed system packages and find a library installed by
        the system package manager that provides the specified soname. All
        the symbols provided by the library found soname will be discarded
        from the provided list of symbols.

        Args:
            soname: SONAME of the shared library to find
            symbols: list of symbols used that have not be attributed to any
                library.

        Return: the system dependency found
        """
        raise NotImplementedError

    def find_pypkg_sysdep(self, pypkg: str) -> str:
        """
        Get sysdep providing the specified python package
        """
        raise NotImplementedError
