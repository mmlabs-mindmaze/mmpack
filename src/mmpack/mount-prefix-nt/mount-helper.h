/*
 * @mindmaze_header@
 */
#ifndef MOUNT_HELPER_H
#define MOUNT_HELPER_H

#include <windows.h>

int prefix_mount_setup_child(PROCESS_INFORMATION* procinfo);
void prefix_mount_process_startup(void);


#endif
