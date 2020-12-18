#include <set>
#include <string>

#include "boost/filesystem.hpp"

#include "pkg/dep.h"

namespace pkg {

std::set<dep> read_deps(boost::filesystem::path const& deps_root, dep const* d,
                        bool recursive = false);
std::set<dep> read_deps(boost::filesystem::path const& deps_root,
                        std::string const& file_content);

}  // namespace pkg
