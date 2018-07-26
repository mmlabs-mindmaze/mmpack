/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>
#include "mmpack-common.h"


/**
 * enum - list of possible package state
 * @MMPACK_PKG_UNSET:     unset package state
 * @MMPACK_PKG_INSTALLED: package has already been installed by mmpack
 * @MMPACK_PKG_STAGED:    package will be installed by mmpack
 * @SYSTEM_PKG_INSTALLED: package has already been installed by the system
 * @SYSTEM_PKG_REQUIRED:  package is missing from the system
 */
enum {
	MMPACK_PKG_UNSET = 0,
	MMPACK_PKG_INSTALLED,
	MMPACK_PKG_STAGED,
	SYSTEM_PKG_INSTALLED,
	SYSTEM_PKG_REQUIRED,
};

int pkg_version_compare(char const * v1, char const * v2);
int get_local_system_install_state(char const * name, char const * version);

#endif /* PACKAGE_UTILS_H */
