/*
 * @mindmaze_header@
 */
#ifndef MMPACK_MOUNT_PREFIX_H
#define MMPACK_MOUNT_PREFIX_H

struct mount_mmpack_dispatch {
	int (*setup)(WCHAR* prefix_path);
};

#endif  /* ifndef MMPACK_MOUNT_PREFIX_H */
