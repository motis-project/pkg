#include "pkg/load_deps.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/interprocess/sync/file_lock.hpp"
#include "boost/interprocess/sync/scoped_lock.hpp"

#include "fmt/format.h"

#include "utl/parser/file.h"
#include "utl/to_set.h"

#include "cista/hashing.h"

#include "pkg/dependency_loader.h"
#include "pkg/exec.h"
#include "pkg/git.h"
#include "pkg/read_deps.h"

namespace fs = boost::filesystem;

namespace pkg {

void load_deps(fs::path const& repo, fs::path const& deps_root,
               bool const clone_https, bool const force, bool const recursive,
               bool const drop_wip) {
  if (!boost::filesystem::is_directory(deps_root)) {
    boost::filesystem::create_directories(deps_root);
  }

  auto const iterator = [&](dep* d, branch_commit const& bc) {
    if (fs::is_directory(d->path_)) {
      executor e;
      try {
        // Fetch if commit is not known.
        if (!commit_exists(d, d->commit_) ||
            (d->commit_ != bc.commit_ && !commit_exists(d, bc.commit_))) {
          fmt::print("{}: fetch\n", d->name());
          std::cout << std::flush;
          e.exec(d->path_, "git remote set-url origin {}",
                 url_to_protocol(
                     d->url_, clone_https ? protocol::kHttps : protocol::kSsh));
          if (clone_https) {
            // Push needs to use ssh even with http
            e.exec(d->path_, "git remote set-url --push origin {}",
                   url_to_protocol(d->url_, protocol::kSsh));
          }
          e.exec(d->path_, "git fetch origin");
        }

        // Select latest known commit.
        if (d->commit_ != bc.commit_ &&
            commit_time(d, d->commit_) < commit_time(d, bc.commit_)) {
          d->commit_ = bc.commit_;
          d->branch_ = bc.branch_;
        }
      } catch (std::exception const& ex) {
        fmt::print("Rev-Update failed for {}: {}\n", d->name(), ex.what());
        e.print_trace();
      }
    } else {
      fmt::print("cloning: {}\n", d->name());
      std::cout << std::flush;

      std::function<void(int)> clone_retry = [&](int i) {
        executor e;
        try {
          git_clone(e, d, clone_https);
        } catch (std::exception const& ex) {
          fmt::print("Repo checkout failed for {}: {}\n", d->name(), ex.what());
          e.print_trace();

          std::cout << "Retry clone\n";
          if (i != 0) {
            clone_retry(i - 1);
          }
        }
      };

      clone_retry(2);
    }
  };

  auto const hash = [&]() {
    auto const pred = dep::root(repo);
    auto h = cista::BASE_HASH;
    for (auto const& d : read_deps(deps_root, &pred, recursive)) {
      h = cista::hash_combine(h, cista::hash(d.name()), cista::hash(d.commit_));
    }
    return h;
  }();

  auto const name = (fs::absolute(repo) / ".pkg.mutex").generic_string();
  { auto const create_file_if_not_exists = std::ofstream{name}; }
  auto lock = boost::interprocess::file_lock{name.c_str()};
  if (!lock.try_lock()) {
    std::cout << "waiting for lock" << std::endl;
  } else {
    lock.unlock();
  }
  auto const guard = boost::interprocess::scoped_lock{lock};

  try {
    auto f = std::fstream{};
    f.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    f.open((repo / ".pkg.lock").generic_string().c_str(),
           std::ios_base::in | std::ios_base::out);

    auto lock_hash = cista::hash_t{};
    f >> lock_hash;

    auto const check_lock_file = [&]() {
      auto dep = std::string{};
      auto commit = std::string{};
      while (!f.eof() && f.peek() != EOF && f >> dep >> commit) {
        std::string line;
        std::getline(f, line);
        if (get_commit(deps_root / dep) != commit) {
          return false;
        }
      }
      return true;
    };

    if (lock_hash == hash && check_lock_file()) {
      return;
    }
  } catch (std::exception const& e) {
  }

  dependency_loader l{deps_root};
  auto repeat = false;
  do {
    repeat = false;
    l.retrieve(repo, iterator, recursive);
    for (auto const& d : l.get_all()) {
      if (d->url_ == ROOT || d->commit_ == get_commit(d->path_)) {
        continue;
      }
      executor ex;
      try {
        if (!drop_wip) {
          auto const remote_refs =
              ex.exec(d->path_,
                      "git branch -r --contains HEAD --format '%(refname)'")
                  .out_;
          auto const num_refs = std::ranges::count(remote_refs, '\n');
          if (num_refs == 0) {
            fmt::println(
                "warning: {} has commits that have not been pushed yet. "
                "Use --drop-wip to check out the commit from .pkg "
                "nevertheless.",
                d->path_.string());
            continue;
          }
        }

        git_attach(ex, d, force);
        fmt::print("{}: checkout {}\n", d->name(), git_shorten(d, d->commit_));
        repeat = true;
      } catch (std::exception const& e) {
        fmt::print("Checkout failed for {}: {}\n", d->name(), e.what());
        ex.print_trace();
        throw;
      }
    }
  } while (repeat);

  {
    std::ofstream of{(deps_root / "CMakeLists.txt").generic_string().c_str()};
    of << "cmake_minimum_required(VERSION 3.10)\n"
       << "project(" + deps_root.string() << ")\n\n";
    for (auto const& v : l.sorted()) {
      if (v->url_ == ROOT) {
        continue;
      }
      of << "add_subdirectory(" << v->name() << " EXCLUDE_FROM_ALL)\n";
    }
  }

  {
    std::ofstream of{(repo / ".pkg.lock").generic_string().c_str()};
    of << hash << "\n";
    for (auto const& v : l.sorted()) {
      if (v->url_ == ROOT) {
        continue;
      }
      of << v->name() << " " << v->commit_ << "\n";
    }
  }
}

}  // namespace pkg
