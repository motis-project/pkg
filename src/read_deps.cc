#include "pkg/read_deps.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

#include "boost/property_tree/ini_parser.hpp"
#include "boost/property_tree/ptree.hpp"

#include "utl/to_set.h"

namespace fs = boost::filesystem;

namespace pkg {

std::set<dep> read_deps(fs::path const& deps_root,
                        std::string const& file_content) {
  namespace pt = boost::property_tree;

  std::stringstream ss;
  ss.str(file_content);

  pt::ptree tree;
  pt::read_ini(ss, tree);
  return utl::to_set(tree, [&](auto const& entry) {
    auto const& settings = entry.second;
    return dep{deps_root,
               settings.template get<std::string>(pt::path{"url"}, ""),
               settings.template get<std::string>(pt::path{"commit"}, ""),
               settings.template get<std::string>(pt::path{"branch"}, "")};
  });
}

std::optional<std::string> read_file(fs::path const& path) {
  std::ifstream f{path.string().c_str(), std::ios::binary | std::ios::ate};
  f.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  auto const size = f.tellg();
  f.seekg(0, std::ios::beg);

  std::string buffer;
  buffer.resize(size);

  return f.read(&buffer[0], size) ? std::make_optional(buffer) : std::nullopt;
}

std::set<dep> read_deps(fs::path const& deps_root, dep const* d) {
  if (auto const p = d->path_ / PKG_FILE; !fs::is_regular_file(p)) {
    return {};
  } else {
    auto const file_content = read_file(p);
    return file_content ? read_deps(deps_root, *file_content) : std::set<dep>{};
  }
}

}  // namespace pkg