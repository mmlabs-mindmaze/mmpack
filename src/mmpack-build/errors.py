# @mindmaze_header@
"""
Definition of custom errors
"""

class MMPackBuildError(Exception):
    """Basic exception for errors raised by mmpack-build."""
    def __init__(self, msg=None):
        if msg is None:
            msg = "mmpack-build failed"
        super().__init__(msg)


class ShellException(MMPackBuildError):
    """Custom exception for shell command error."""
