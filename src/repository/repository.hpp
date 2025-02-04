/*
 * @mindmaze_header@
 */
#include <string>
#include <unordered_map>


class BinPkg
{
	std::string data;
	std::string_view name, version, source;
	std::string_view srcsha256, sumsha256sums;

public:
	BinPkg(std::string& data);

	std::string srcid(void) const;
};


class SrcPkg
{
	std::string data;
	std::string srcid;
	int nref;

public:
	SrcPkg(std::string& data);
};


class IndicesStates
{
	std::unordered_map<std::string_view, BinPkg> binindex;
	std::unordered_map<std::string_view, SrcPkg> srcindex;
public:
	IndicesStates(void) {
			
	}
};


class Repository
{
	std::string repodir;
	std::string arch;
	std::unordered_map<std::string_view, BinPkg> binindex;
	std::unordered_map<std::string, BinPkg> srcindex;

public:
	Repository(std::string repodir, std::string arch);
	IndicesStates begin_change(void);
	
};
