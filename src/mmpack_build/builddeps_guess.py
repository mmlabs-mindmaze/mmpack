# @mindmaze_header@
"""
Guess build_dependencies from sources
"""

from copy import deepcopy
from typing import Any, Iterator
from configparser import ConfigParser

try:
    from tomllib import load as toml_load
except ImportError:
    from tomli import load as toml_load

from pkg_resources import Requirement

from .source_strap_specs import SourceStrapSpecs


class _BuilddepsGuess:
    def __init__(self, cfg: dict[str, Any]):
        self._ignore = set(cfg.get('ignore', []))
        self._remap = cfg.get('remap', {})

    def _iter_guessed_used_builddeps(self, srcdir: str) -> Iterator[str]:
        raise NotImplementedError

    def _get_mmpack_dep(self, used: str) -> str:
        raise NotImplementedError

    def guess(self, srcdir: str) -> set[str]:
        """Get the set of mmpack package guessed to be build dependencies."""
        used_deps = set(self._iter_guessed_used_builddeps(srcdir))
        used_deps.difference_update(self._ignore)
        return {self._remap.get(u, self._get_mmpack_dep(u))
                for u in used_deps}


def _iter_pyproject_builddeps(srcdir: str) -> list[str]:
    try:
        with open(f'{srcdir}/pyproject.toml', 'rb') as stream:
            data = toml_load(stream)
    except FileNotFoundError:
        return []

    return (data.get('build-system', {}).get('requires', [])
            + data.get('project', {}).get('dependencies', []))


def _iter_setupcfg_builddeps(srcdir: str) -> list[str]:
    try:
        config = ConfigParser()
        config.read(f'{srcdir}/setup.cfg')
        deps_field = config['options']['install_requires']
    except (FileNotFoundError, KeyError):
        return []

    return deps_field.strip().split('\n')


class _BuilddepsGuessPython(_BuilddepsGuess):

    def _iter_guessed_used_builddeps(self, srcdir: str) -> Iterator[str]:
        dep_strings = []
        dep_strings += _iter_pyproject_builddeps(srcdir)
        dep_strings += _iter_setupcfg_builddeps(srcdir)

        for dep_string in dep_strings:
            req = Requirement.parse(dep_string)
            yield req.project_name

    def _get_mmpack_dep(self, used: str) -> str:
        name = used.lower().replace('-', '_')
        if name.startswith('python_'):
            name = name[len('python_'):]
        name.translate({ord('_'): '-', ord('.'): '-'})
        return 'python3-' + name


_GUESS_BACKENDS: dict[str, _BuilddepsGuess] = {
    'python': _BuilddepsGuessPython
}


def guess_build_depends(specs: SourceStrapSpecs, srcdir: str) -> set[str]:
    """
    Guess the build depends according to mmpack/sources-strap config

    Arguments:
        specs: specs defined in sources-strap
        srcdir: path to sources
    """
    cfg = deepcopy(specs.get('guess-build-depends', {}))
    from_builders = cfg.pop('from', None)
    if from_builders is None:
        from_builders = list(cfg.keys())
    elif from_builders == 'all':
        from_builders = list(_GUESS_BACKENDS.keys())
    elif isinstance(from_builders, str):
        from_builders = [from_builders]
    elif not isinstance(from_builders, list):
        raise ValueError('guess-build-depends/from key in mmpack/sources-strap'
                         ' must be a list or single string')

    # Run builddeps guess for all listed backends
    guessed_deps = set()
    for builder in from_builders:
        backend_cls = _GUESS_BACKENDS[builder]
        backend = backend_cls(cfg.get(builder, {}))
        guessed_deps.update(backend.guess(srcdir))

    return guessed_deps
