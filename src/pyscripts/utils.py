# [@]mmpack_header[@]
from functools import cache

from astroid.modutils import is_namespace, file_info_from_modpath


@cache
def is_namespace_pkg(modname: str) -> bool:
    """reports true if modname is a PEP420 namespace package"""
    try:
        is_ns = is_namespace(file_info_from_modpath(modname.split('.')))
    except ImportError:
        is_ns = False
    return is_ns

