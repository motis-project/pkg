#include "pkg/load_deps.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "boost/asio/io_service.hpp"
#include "boost/asio/strand.hpp"
#include "boost/filesystem.hpp"

#include "fmt/format.h"

#include "utl/to_set.h"

#include "pkg/dependency_loader.h"
#include "pkg/exec.h"
#include "pkg/git.h"
#include "pkg/read_deps.h"

namespace fs = boost::filesystem;

namespace pkg {

void load_deps(fs::path const& repo, fs::path const& deps_root,
               bool const clone_https, bool const force, bool const recursive) {
  if (!boost::filesystem::is_directory(deps_root)) {
    boost::filesystem::create_directories(deps_root);
  }

  auto const iterator = [&](dep* d, branch_commit const& bc) {
    if (fs::is_directory(d->path_)) {
      executor e;
      try {
        // Fetch if commit is not known.
        if (!commit_exists(d, d->commit_) || !commit_exists(d, bc.commit_)) {
          fmt::print("{}: fetch\n", d->name());
          std::cout << std::flush;
          e.exec(d->path_, "git fetch origin");
        }

        // Select latest known commit.
        if (commit_time(d, d->commit_) < commit_time(d, bc.commit_)) {
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
        git_attach(ex, d, force);
        fmt::print("{}: checkout {}\n", d->name(), git_shorten(d, d->commit_));
        repeat = true;
      } catch (std::exception const& e) {
        fmt::print("Checkout failed for {}: {}\n", d->name(), e.what());
        ex.print_trace();
      }
    }
  } while (repeat);

  std::ofstream of{"deps/CMakeLists.txt"};
  of << "cmake_minimum_required(VERSION 3.10)\n"
     << "project(" + deps_root.string() << ")\n\n";
  for (auto const& v : l.sorted()) {
    if (v->url_ == ROOT) {
      continue;
    }
    of << "add_subdirectory(" << v->name() << " EXCLUDE_FROM_ALL)\n";
  }
}

}  // namespace pkg
