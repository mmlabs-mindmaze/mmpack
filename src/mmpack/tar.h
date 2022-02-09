/*
 * @mindmaze_header@
 */
#ifndef TAR_H
#define TAR_H


#include <archive.h>
#include <archive_entry.h>


struct tarstream {
	struct archive* archive;
	const char* filename;
	struct archive_entry* entry;
	const char* entry_path;
	int entry_type;
};

#define READ_ARCHIVE_EOF 1


int tarstream_open(struct tarstream* tar, const char* filename);
int tarstream_close(struct tarstream* tar);
int tarstream_read_next(struct tarstream* tar);
int tarstream_extract(struct tarstream* tar, const char* path);

int tar_load_file(const char* filename, const char* archive_path,
                  struct buffer* buffer);

#endif  /* TAR_H */
