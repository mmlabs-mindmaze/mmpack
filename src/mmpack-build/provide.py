# @mindmaze_header@
"""
common classes to specify provided symbols to other packages
"""
from __future__ import annotations

import re
from os import listdir
from os.path import exists
from typing import (Set, Dict, Tuple, List, NamedTuple, Iterable, Iterator,
                    Optional)

from .common import wprint, yaml_serialize, yaml_load
from .mm_version import Version
from .package_info import PackageInfo
from .workspace import Workspace


# pylint: disable=too-few-public-methods
class ProvidedSymbol:
    """
    data describing a symbol provided by a library known from its name key
    (referenceable in spec file) and the actual full symbol name
    """
    __slots__ = ('symbol', 'name')

    def __init__(self, name: str, symbol: Optional[str] = None):
        if not symbol:
            symbol = name
        self.name = name
        self.symbol = symbol

    def __repr__(self):
        return f'(name={self.name}, sym={self.symbol})'


class _SpecSymbol(NamedTuple):
    name: str
    version: str
    tags: List[str]


class _SpecsSymbols:
    def __init__(self, symbols: Optional[Dict[str, Version]],
                 provided_syms: Optional[Iterable[ProvidedSymbol]] = None):
        self._older = []  # type: List[ProvidedSymbol]
        self._removed = set()
        self._symbols = symbols if symbols else {}
        self._new_syms = {s.name for s in (provided_syms or [])}

    def known_symbols(self) -> Iterator[_SpecSymbol]:
        """
        provides a iterator of the symbol specified in the spec file.
        """
        for keyopt, version in self._symbols.items():
            # Parse possible tags in front key in the form: '(tag1,tag2)symkey'
            if keyopt.startswith('('):
                tagstr, key = keyopt[1:].split(')', 1)
                tags = tagstr.split(',')
            else:
                key = keyopt
                tags = []

            yield _SpecSymbol(name=key, version=version, tags=tags)

    def mark_older(self, specsym: _SpecSymbol):
        """
        mark symbol as provided in a earlier version than expected in the spec
        """
        self._older.append(specsym)

    def mark_used(self, provsym: ProvidedSymbol):
        """
        mark the symbol as used
        """
        self._new_syms.discard(provsym.name)

    def mark_removed(self, sym: _SpecSymbol):
        """
        mark the symbol found in the specs, absent from the provided symbol of
        library
        """
        self._removed.add(sym.name)

    def report_changes(self):
        """
        Print warning log regarding:
             - new symbol have been found, not listed in spec
             - symbols appearing in earlier version than expected
             - symbol removed in new version
        """
        if self._new_syms:
            wprint('The following symbols were found but not specified:\n\t'
                   + '\n\t'.join(sorted(self._new_syms)))
            wprint('They will all be considered as introduced in the current'
                   ' project version.')

        if not self._older:
            oldsyms = [f'{s.name}' for s in self._older]
            wprint('The following symbols have been introduced before than'
                   ' expected:\n\t' + '\n\t'.join(sorted(oldsyms)))

        if self._removed:
            wprint('The following symbols appear to have been removed:\n\t'
                   + '\n\t'.join(sorted(self._removed)))


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

    def _get_decorated_symbols(self) -> List[ProvidedSymbol]:
        return [ProvidedSymbol(s) for s in self.symbols.keys()]

    def add_symbols(self, symbols: Set[str],
                    version: Version = Version('any')) -> None:
        """
        add a set of symbol along with a minimal version.
        """
        self.symbols.update(dict.fromkeys(symbols, version))

    def update_from_specs(self, pkg_specs: dict,
                          pkg: PackageInfo) -> _SpecsSymbols:
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
            pkg: binary package being built.

        Returns:
            _SpecSymbols holding the symbols specs associated with the package
            name.
        """
        # Update package deps associated with the provide only if specified
        self.pkgdepends = pkg_specs.get('depends', self.pkgdepends)

        provided_syms = self._get_decorated_symbols()
        specs_symbols = _SpecsSymbols(pkg_specs.get('symbols'), provided_syms)

        # Generate a lookup table with all provided symbols keyed by both the
        # full symbol name and the reduced symbol name. If 2 symbols share the
        # same key, priority will be given to the symbol whose reduced name is
        # same as symbol
        lut_syms = {s.name: s for s in provided_syms}
        lut_syms.update({s.symbol: s for s in provided_syms})

        # Update minimal version associated with symbol for all symbols found
        # in dep_specs['symbols'] dictionary
        for specsym in specs_symbols.known_symbols():
            if 'regex' in specsym.tags:
                pattern = re.compile(specsym.name)
                matchsyms = [s for k, s in lut_syms.items()
                             if pattern.fullmatch(k)]
            else:
                # Find full symbol name with matching key
                sym = lut_syms.get(specsym.name)
                matchsyms = [sym] if sym else []

            # No match, report only in symbol in spec is not optional or more
            # recent than package being built (may append with ghost)
            if not matchsyms:
                if 'optional' not in specsym.tags:
                    version = specsym.version
                    if version == 'CURRENT' or Version(version) <= pkg.version:
                        specs_symbols.mark_removed(specsym)
                continue

            for provsym in matchsyms:
                specs_symbols.mark_used(provsym)
                provsym_version = self.symbols[provsym.symbol]
                version = Version(specsym.version) \
                    if specsym.version != 'CURRENT' else provsym_version
                if version <= provsym_version:
                    self.symbols[provsym.symbol] = version
                else:  # version > self.version:
                    specs_symbols.mark_older(provsym)

        return specs_symbols


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

        yaml_serialize(data, filename, use_block_style=True)

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

    def update_from_specs(self, pkg_spec_provide: Dict,
                          pkg: PackageInfo) -> None:
        """
        Update the ProvideList symbols dictionary based on content of
        specs. specs should be the result of loading the yaml contained in
        <pkgname>.provide file of the package.

        Args:
            pkg_spec_provide: dict representing <pkgname>.provide content.
            pkg: binary package being built.

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
                             'provided'.format(self.type, pkg.name))

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
            pkg_specs = specs.get(provide.name, {})
            specs_syms = provide.update_from_specs(pkg_specs, pkg)
            specs_syms.report_changes()

    def _get_dep_minversion(self, soname: str,
                            symbols: Set[str]) -> Tuple[str, Version]:
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

    def resolve_deps(self, currpkg: PackageInfo,
                     sonames: Set[str], symbols: Set[str],
                     same_srcpkg: bool = False):
        """
        For each soname used try to find the dependency to use in the
        provide list. For each found, the soname and its associated symbols
        are discarded from the soanames and symbols set given in input

        Args:
            currpkg: PackageInfo to which the dependency must be added
            sonames: list of sonanme that must be searched in ProvideList
            symbols: set of used symbols to update if any soname is found
            same_srcpkg: if true, assumes the dependency found is built along
                with currpkg.
        """
        for soname in list(sonames):
            pkg, version = self._get_dep_minversion(soname, symbols)
            if pkg:
                if same_srcpkg:
                    versions = (currpkg.version, currpkg.version)
                else:
                    versions = (version,)

                currpkg.add_to_deplist(pkg, *versions)
                sonames.remove(soname)


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
    metadatadir = wrk.prefix + '/var/lib/mmpack/metadata/'
    pattern = re.compile(r'.*.{}(?:.gz)?'.format(extension))

    provides = ProvideList(symtype)

    if not exists(metadatadir):
        return provides

    for symfile in listdir(metadatadir):
        if pattern.fullmatch(symfile):
            provides.add_from_file(metadatadir + symfile)

    return provides


def pkgs_provides(pkgs: Iterable[PackageInfo], ptype: str) -> ProvideList:
    """Create union of Provides of each iterated package

    Args:
        pkgs: iterator of package
        ptype: provide type to select

    Return:
        A ProvideList merging all package provides
    """
    res = ProvideList('')
    for pkg in pkgs:
        providelist = pkg.provides[ptype]
        res.type = providelist.type
        # pylint: disable=protected-access
        res._provides.update(providelist._provides)

    return res
