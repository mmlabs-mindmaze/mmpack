/*
 * @mindmaze_header@
 */
#include <string>


class Repository
{
	std::string repodir;
	std::string arch;

	void setup(void);

public:
	Repository(std::string repodir, std::string arch) : repodir(repodir), arch(arch)
	{
		setup();
	}
};
