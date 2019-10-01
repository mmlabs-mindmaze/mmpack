#include <stdio.h>

#define TESTFILE LIBEXECDIR"/libexec-world"EXEEXT

int main(void)
{
	size_t i, n;
	unsigned char buffer [16];
	FILE * fp = fopen(TESTFILE, "rb");
	if (fp == NULL) {
		fprintf(stderr, "could not find %s\n", TESTFILE);
		return -1;
	}

	n = 0;
	while (!feof(fp) && n++ < 10 )
	{
		if (fgets ((char *)buffer , sizeof(buffer) , fp) == NULL )
			break;

		for (i = 0 ; i < sizeof(buffer) ; i++) {
			printf("%02x ", buffer[i]);
		}
		printf("\n");
	}

	fclose (fp);
	return 0;
}
