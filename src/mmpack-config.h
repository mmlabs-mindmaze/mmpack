/*
 * @mindmaze_header@
 */
#ifndef MMPACK_CONFIG_H
#define MMPACK_CONFIG_H

typedef int (*server_cb)(char const*, char const*, void*);

char const * get_config_filename(void);
int foreach_config_server(char const * filename, server_cb cb, void * arg);

#endif /* MMPACK_CONFIG_H */
