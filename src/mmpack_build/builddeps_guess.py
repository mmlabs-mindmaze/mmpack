# @mindmaze_header@
"""
Guess build_dependencies from sources
"""

from copy import deepcopy
from typing import Any, Iterator

from .source_strap_specs import SourceStrapSpecs


class _BuilddepsGuess:
    def __init__(self, cfg: dict[str, Any]):
        self._ignore = set(cfg.get('ignore'))
        self._remap = cfg.get('remap')

    def _iter_guessed_used_builddeps(self, srcdir: str) -> Iterator[str]:
        raise NotImplementedError

    def _get_mmpack_dep(self, used: str) -> str:
        raise NotImplementedError

    def guess(self, srcdir: str) -> set[str]:
        used_deps = set(self._iter_guessed_used_builddeps())
        used_deps.difference_update(self._ignore)
        return {self._remap.get(u, self._get_mmpack_dep(u))
                for u in used_deps}


_GUESS_BACKENDS: dict[str, _BuilddepsGuess] = {
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
        from_builders = list(from_builders)
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
