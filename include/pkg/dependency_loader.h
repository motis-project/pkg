#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "boost/filesystem/path.hpp"

#include "pkg/dep.h"

namespace pkg {

struct dependency_loader {
public:
  using iteration_fn_t = std::function<void(dep*, branch_commit const&)>;
  using async_iteration_fn_t = std::function<void(dep*, iteration_fn_t)>;

  explicit dependency_loader(boost::filesystem::path deps_root);
  ~dependency_loader();

  void retrieve(
      boost::filesystem::path const&,
      iteration_fn_t const& = [](dep*, branch_commit const&) {},
      bool recursive = false);

  dep* root() const;
  std::vector<dep*> sorted() const;
  std::vector<dep*> get_all() const;
  std::optional<dep*> resolve(std::string const& url) const;

private:
  void retrieve(dep* pred, iteration_fn_t const&, bool recursive);

  boost::filesystem::path deps_root_;

  // One dep per directory (URLs with the same name share it), keyed by name.
  std::map<std::string, dep*> deps_;
  std::vector<std::unique_ptr<dep>> dep_mem_;

  // URLs from the root's .pkg by name, they win over other references.
  std::map<std::string, std::string> root_urls_;
};

}  // namespace pkg