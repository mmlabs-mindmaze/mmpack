# @mindmaze_header@
"""
common classes to specify provided symbols to other packages
"""

from glob import glob
from typing import Set, Dict, Tuple, List, Optional

from . common import wprint, yaml_serialize, yaml_load
from . mm_version import Version
from . workspace import Workspace


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

    def _get_symbol(self, name: str):
        return self.symbols.get(name)

    def add_symbols(self, symbols: Set[str],
                    version: Version = Version('any')) -> None:
        """
        add a set of symbol along with a minimal version.
        """
        self.symbols.update(dict.fromkeys(symbols, version))

    def _get_symbols_keys(self):
        return set(self.symbols.keys())

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
            curr_version = self._get_symbol(name)
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

        # if a specs file is provided, but is incomplete, display a warning
        diff = self._get_symbols_keys() - set(specs_symbols.keys())
        if diff:
            wprint('The following symbols were found but not specified:\n\t'
                   + '\n\t'.join(diff))
            wprint('They will all be considered as introduced in the current'
                   'project version.')


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

    def get(self, soname) -> Optional[Provide]:
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

    def add_from_file(self, filename) -> None:
        """
        Load provides by reading a file and add them to current
        """
        metadata = yaml_load(filename)
        for name, sodata in metadata.items():
            provide = Provide(name)
            provide.pkgdepends = sodata['depends']
            provide.symbols = {sym: Version(version)
                               for sym, version in sodata['symbols'].items()}
            self.add(provide)

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

    def _get_dep_minversion(self, soname: str, symbols: Set[str]) \
            -> Tuple[Optional[str], Version]:
        """
        Compute the package dependency given a soname and set of symbols.
        If the soname is provided by the ProvideList, the symbols exported
        by will be removed from the symbols set given on argument.
        Moreover, this will determine the minimal version to be used given
        the associated used symbol.

        Args:
            soname: soname that must be search in ProvideList
            symbols: set of used symbols to update if soname is found

        Returns:
            If soname is found, a tuple containing package name and the minimal
            version to use, (None, None) otherwise.
        """
        provide = self._provides.get(soname)
        if not provide:
            return (None, Version(None))

        min_version = None
        for sym in list(symbols):  # iterate over a shallow copy of the list
            if sym in provide.symbols:
                metadata_version = provide.symbols[sym]
                if min_version:
                    min_version = max(min_version, metadata_version)
                else:
                    min_version = metadata_version
                symbols.remove(sym)  # remove symbol from the list

        if not min_version:
            min_version = Version(None)

        return (provide.pkgdepends, min_version)

    def gen_deps(self, sonames: Set[str],
                 symbols: Set[str]) -> List[Tuple[str, Version]]:
        """
        For each soname used try to find the dependency to use in the
        provide list. For each found, the soname and its associated symbols
        are discarded from the soanames and symbols set given in input

        Args:
            sonames: list of sonanme that must be searched in ProvideList
            symbols: set of used symbols to update if any soname is found

        Returns:
            list of tuple of package and min version to be added to the
            dependencies.
        """
        dep_list = []
        for soname in list(sonames):
            pkg, version = self._get_dep_minversion(soname, symbols)
            if pkg:
                dep_list.append((pkg, version))
                sonames.remove(soname)

        return dep_list


def load_mmpack_provides(extension: str, symtype) -> ProvideList:
    """
    Load all the provides of one type from all installed packages in prefix

    Args:
        extension: extension of the files that contains the data regarding
            provided symbols of the matching type for each mmpack packages
        symtype: symbol type whose provided symbols data must be loaded

    Returns:
        ProvideList representing the database of all exported symbols by all
        installed mmpack packages matching symtype
    """
    wrk = Workspace()
    symfiles = glob('{}/var/lib/mmpack/metadata/**.{}'
                    .format(wrk.prefix, extension))

    provides = ProvideList(symtype)
    for symfile in symfiles:
        provides.add_from_file(symfile)

    return provides
