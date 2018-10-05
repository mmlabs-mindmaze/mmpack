
AC_DEFUN([AC_SET_HOSTSYSTEM],
[AC_REQUIRE([AC_CANONICAL_HOST])
AS_CASE([$host],
        [*win32* | *mingw* | *windows*], [os_system=win32],
        [os_system=posix])
AS_CASE([$host],
        [*win32* | *mingw* | *windows*], [os_kernel=nt],
        [*linux*], [os_kernel=linux])
AM_CONDITIONAL([OS_KERNEL_LINUX], [test "$os_kernel" = linux])
AM_CONDITIONAL([OS_KERNEL_NT], [test "$os_kernel" = nt])
AM_CONDITIONAL([OS_TYPE_POSIX], [test "$os_system" = posix])
AM_CONDITIONAL([OS_TYPE_WIN32], [test "$os_system" = win32])
])
