#include "pkg/print_status.h"

#include <functional>

#include "fmt/color.h"

#include "utl/to_set.h"

#include "pkg/dependency_loader.h"
#include "pkg/git.h"
#include "pkg/update_deps.h"

namespace fs = boost::filesystem;

namespace pkg {

void resolve_conflict(dep* d) {
  auto const infos = get_commit_infos(
      d->path_, utl::to_set(d->referenced_commits_,
                            [](auto&& entry) { return entry.first; }));
  auto const latest = infos.front().bc_;
  d->commit_ = latest.commit_;
  d->branch_ = latest.branch_;

  fmt::print("Conflict in dependency {} [upgrade to {}]:\n", d->name(),
             d->commit_);

  auto i = 0U;
  for (auto const& info : infos) {
    auto const ff = is_fast_forward(d->path_, info.bc_.commit_, latest.commit_);
    fmt::print("  {} ", i == 0U ? "            "
                        : ff    ? "FAST-FORWARD"
                                : "ERROR       ");

    fmt::print(" {} on {} ({}) referenced by ", info.bc_.commit_,
               info.bc_.branch_, info.info_);
    for (auto const& pred : d->referenced_commits_[info.bc_]) {
      fmt::print("{} ", pred->name());
    }

    fmt::print("\n");

    ++i;
  }
}

void resolve_conflicts(fs::path const& repo, fs::path const& deps_root) {
  dependency_loader l{deps_root};
  l.retrieve(repo);

  for (auto const& d : l.get_all()) {
    if (d->referenced_commits_.size() > 1) {
      resolve_conflict(d);
    }
  }

  update_deps(l.sorted(), false);
}

}  // namespace pkg