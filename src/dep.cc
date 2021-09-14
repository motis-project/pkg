#include "pkg/dep.h"

#include <fstream>
#include <ostream>

#include "pkg/detect_branch.h"
#include "pkg/git.h"
#include "pkg/name_from_url.h"

namespace pkg {

dep::dep(boost::filesystem::path const& deps_root, std::string url,
         std::string commit, std::string branch)
    : path_{deps_root / name_from_url(url)},
      url_{std::move(url)},
      commit_{std::move(commit)},
      branch_{std::move(branch)} {}

void dep::write_pkg_file() const {
  std::vector<dep*> succs{begin(succs_), end(succs_)};
  std::sort(begin(succs), end(succs),
            [](auto const* a, auto const* b) { return a->name() < b->name(); });

  std::ofstream f{pkg_file().string().c_str()};
  for (auto const s : succs) {
    detect_branch(s);
    f << "[" << s->name() << "]\n"  //
      << "  url=" << s->url_ << "\n"  //
      << "  branch=" << s->branch_ << "\n"  //
      << "  commit=" << s->commit_ << "\n";
  }
}

dep dep::root(boost::filesystem::path const& root_repo) {
  dep d;
  d.path_ = root_repo;
  d.url_ = ROOT;
  d.branch_ = ROOT;
  return d;
}

std::set<dep*> dep::recursive_preds() const {
  auto deps = std::set<dep*>{};
  auto todo = std::set<dep*>{};
  todo.insert(begin(preds_), end(preds_));

  while (!todo.empty()) {
    auto const d = *begin(todo);
    todo.erase(begin(todo));
    todo.insert(begin(d->preds_), end(d->preds_));
    deps.emplace(d);
  }

  return deps;
}

bool dep::is_root() const { return url_ == ROOT; }

boost::filesystem::path dep::pkg_file() const { return path_ / PKG_FILE; }

std::string dep::name() const { return name_from_url(url_); }

}  // namespace pkg
