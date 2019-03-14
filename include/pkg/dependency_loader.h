#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem/path.hpp"

#include "utl/parser/util.h"

#include "pkg/dep.h"

namespace pkg {

struct dependency_loader {
public:
  using iteration_fn_t = std::function<void(boost::filesystem::path, dep*)>;

  explicit dependency_loader(boost::filesystem::path deps_root);
  ~dependency_loader();

  void retrieve(boost::filesystem::path const&,
                iteration_fn_t const& = [](boost::filesystem::path const&,
                                           dep*) {});
  std::vector<dep*> sorted();

private:
  void retrieve(dep* pred, iteration_fn_t const&);

  boost::filesystem::path deps_root_;
  std::map<std::string, dep*> deps_;
  std::vector<std::unique_ptr<dep>> dep_mem_;
};

}  // namespace pkg