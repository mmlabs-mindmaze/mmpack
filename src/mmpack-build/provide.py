# @mindmaze_header@
"""
common classes to specify provided symbols to other packages
"""

from typing import Dict, Set

from common import yaml_serialize
from mm_version import Version


class Provide:
    """
    container class list the provided symbol and libname. This container
    should be suitable to any type of symbol (elf, pe, python...).
    """

    def __init__(self, name: str, soname: str = None):
        self.name = name
        self.soname = soname if soname else name
        self.pkgdepends = None
        self.symbols = dict()

    def add_symbols(self, symbols: Set[str],
                    version: Version = Version('any')) -> None:
        """
        add a set of symbol along with a minimal version.
        """
        self.symbols.update(dict.fromkeys(symbols, version))

    def update_from_specs(self, pkg_specs: dict) -> None:
        """
        Update minimal version associated with each symbol. This is
        typically used to integrate the data in package provide files.

        Args:
            pkg_specs: dict with the keys 'symbols' and 'depends'. The value
                associated to 'symbols' must be a dict of symbols -> min
                version mapping associated to the provided soname as specified
                in the package provides file. The value associated with
                'depends' must be a string describing the package dependency
                that must be pulled. Both keys ('depends' and 'symbols') are
                optional, but if is present, it must follow the described
                structure.

        Returns:
            None

        Raises:
            ValueError: a symbol specified in pkg_specs['symbols'] is not found
                in self.symbols or the associated version is more recent than
                the current version.
        """
        # Update package deps associated with the provide only if specified
        self.pkgdepends = pkg_specs.get('depends', self.pkgdepends)

        # Update minimal version associated with symbol for all symbols found
        # in dep_specs['symbols'] dictionary
        specs_symbols = pkg_specs.get('symbols', dict())
        for name, str_version in specs_symbols.items():
            # type conversion will raise an error if malformed
            curr_version = self.symbols.get(name)
            if not curr_version:
                raise ValueError('Specified symbol {0} not found '
                                 'in package files'.format(name))

            version = Version(str_version)
            if version <= curr_version:
                self.symbols[name] = version
            else:  # version > self.version:
                raise ValueError('Specified version of symbol {0} ({1})'
                                 'is greater than current version ({2})'
                                 .format(name, version, curr_version))


class ProvideList:
    """
    Container multiple provides of the same symbol type
    """

    def __init__(self, symbol_type: str):
        self.type = symbol_type
        self._provides = dict()

    def add(self, provide: Provide) -> None:
        """
        Add a provide instance to the list
        """
        self._provides[provide.soname] = provide

    def get(self, soname) -> Provide:
        """
        return the provide associated to a soname (if available).
        None otherwise.
        """
        return self._provides.get(soname)

    def serialize(self, filename: str) -> None:
        """
        write the serialized version of the provide list.
        """
        # empty provide list should not generate file
        if not self._provides:
            return

        data = dict()
        for provide in self._provides.values():
            data[provide.soname] = {'depends': provide.pkgdepends,
                                    'symbols': provide.symbols}

        yaml_serialize(data, filename)

    def update_from_specs(self, pkg_spec_provide: Dict, pkgname: str) -> None:
        """
        Update the ProvideList symbols dictionary based on content of
        specs. specs should be the result of loading the yaml contained in
        <pkgname>.provide file of the package.

        Args:
            pkg_spec_provide: dict representing <pkgname>.provide content.
            pkgname: name of the binary package being built.

        Returns:
            None

        Raises:
            ValueError: symbol type is specified in <pkgname>.provide but no
                such symbol type is provided by package, a symbol specified in
                provide file is not found in exported symbol or the associated
                version is more recent than the current version.
        """
        specs = pkg_spec_provide.get(self.type)
        if not specs:
            return

        num_provides = len(self._provides)
        if num_provides == 0:
            raise ValueError('provides specified for type {} in '
                             'package {} but no such symbols are '
                             'provided'.format(self.type, pkgname))

        # If there is only one provide of the type, it is allowed that
        # the provide in specs list directly the exported symbols. In
        # such case, the depends name and soname are assumed to be the
        # same as the (only) provide
        if num_provides == 1:
            # get the only provide name (not soname)
            key = list(self._provides.values())[0].name

            # Check specs has the expected layout, if not, assume it contains
            # directly the dictionary of symbols
            if key not in specs or 'symbols' not in specs[key]:
                specs = {key: {'symbols': specs}}

        for provide in self._provides.values():
            provide.update_from_specs(specs.get(provide.name, dict()))
