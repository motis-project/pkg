#include "pkg/read_deps.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

#include "boost/filesystem.hpp"
#include "boost/property_tree/ini_parser.hpp"
#include "boost/property_tree/ptree.hpp"

#include "utl/pipes.h"
#include "utl/to_set.h"

namespace fs = boost::filesystem;

namespace pkg {

std::set<fs::path> collect_pkg_files(fs::path const& p) {
  std::set<fs::path> q, pkg_files;
  q.emplace(p);

  while (!q.empty()) {
    auto const curr = *q.begin();
    q.erase(q.begin());

    if (curr.filename() == PKG_FILE) {
      pkg_files.emplace(curr);
    } else if (fs::is_directory(curr) && curr.filename() != ".git") {
      for (auto const& dir_entry :
           utl::all(fs::directory_iterator{curr}, fs::directory_iterator{})) {
        q.emplace(dir_entry.path());
      }
    }
  }

  return pkg_files;
}

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

std::optional<std::string> read_file(fs::path const& path,
                                     bool const recursive) {
  auto const read_single_file = [](fs::path const& p) {
    std::ifstream f{p.string().c_str(), std::ios::binary | std::ios::ate};
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    auto const size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::string buffer;
    buffer.resize(size);

    return f.read(&buffer[0], size) ? std::make_optional(buffer) : std::nullopt;
  };

  auto const read_recursive = [read_single_file](fs::path const& p) {
    std::string buffer;
    for (auto const& entry : collect_pkg_files(p)) {
      if (auto const content = read_single_file(entry); content.has_value()) {
        buffer += *content + "\n";
      }
    }
    return std::make_optional(buffer);
  };

  return recursive ? read_recursive(path) : read_single_file(path);
}

std::set<dep> read_deps(fs::path const& deps_root, dep const* d,
                        bool const recursive) {
  if (auto const p = d->path_ / PKG_FILE; !fs::is_regular_file(p)) {
    return {};
  } else {
    auto const file_content = read_file(recursive ? d->path_ : p, recursive);
    return file_content ? read_deps(deps_root, *file_content) : std::set<dep>{};
  }
}

}  // namespace pkg