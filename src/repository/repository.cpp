/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <string>
#include <utility>

#include "repository.hpp"


static
std::string src_id(std::string_view srcname, std::string_view srcsha256)
{
       	return std::string(srcname) + '_' + std::string(srcsha256);
}


SrcPkg::SrcPkg(std::string& data) : data(std::move(data))
{
	std::string_view name(data);
	std::string_view sha256(data);

	srcid = src_id(name, sha256);
}


BinPkg::BinPkg(std::string& data) : data(std::move(data))
{
	
}


std::string BinPkg::srcid(void) const
{
	return src_id(source, srcsha256);
}


Repository::Repository(std::string repodir, std::string arch)
: repodir(repodir), arch(arch)
{
	
}
