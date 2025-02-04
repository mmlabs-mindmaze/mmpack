/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include <cstdlib>

#include "repository.hpp"


int main(int argc, char* argv[])
{
	Repository repo(".", "amd64");
	return EXIT_SUCCESS;
}
