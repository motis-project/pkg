#include "doctest.h"

#include <fstream>
#include <iterator>
#include <string>

#include "boost/filesystem.hpp"

#include "fmt/format.h"

#include "pkg/dependency_loader.h"
#include "pkg/exec.h"
#include "pkg/git.h"
#include "pkg/load_deps.h"

namespace fs = boost::filesystem;

// Both URLs have the name dep1, they share the directory deps/dep1:
//   root -> dep1 (url1, c1)
//   root -> dep2 -> dep1 (url2, c2)
TEST_CASE("one_dep_per_directory") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto const deps = dir / "deps";
  auto const url1 = std::string{"git@example.com:pkg/dep1.git"};
  auto const url2 = std::string{"git@example.com:fork/dep1.git"};

  auto const write_file = [](fs::path const& p, std::string const& content) {
    std::ofstream{p.generic_string()} << content;
  };
  auto const read_file = [](fs::path const& p) {
    auto in = std::ifstream{p.generic_string()};
    return std::string{std::istreambuf_iterator<char>{in},
                       std::istreambuf_iterator<char>{}};
  };
  auto const commit = [](fs::path const& repo, std::string const& date) {
    pkg::exec(repo, "git add -A");
    pkg::exec(repo,
              "git -c user.name=pkg -c user.email=pkg@example.com commit -q "
              "--allow-empty --date={0} -m {0}",
              date);
    return pkg::get_commit(repo);
  };
  auto const pkg_entry = [](std::string const& name, std::string const& url,
                            std::string const& commit) {
    return fmt::format("[{}]\n  url={}\n  branch=master\n  commit={}\n", name,
                       url, commit);
  };

  // dep1: c2 is newer, c1 is checked out
  fs::create_directories(deps / "dep1");
  pkg::exec(deps / "dep1", "git init -q -b master");
  auto const c1 = commit(deps / "dep1", "2020-01-01T00:00:00");
  auto const c2 = commit(deps / "dep1", "2020-01-02T00:00:00");
  pkg::exec(deps / "dep1", "git checkout -q {}", c1);

  // dep2's URL sorts first: the loader reaches dep1 through dep2 before the
  // root's own reference
  fs::create_directories(deps / "dep2");
  pkg::exec(deps / "dep2", "git init -q -b master");
  write_file(deps / "dep2" / ".pkg", pkg_entry("dep1", url2, c2));
  auto const d2 = commit(deps / "dep2", "2020-01-01T00:00:00");

  write_file(dir / ".pkg",
             pkg_entry("dep1", url1, c1) +
                 pkg_entry("dep2", "git@example.com:a/dep2.git", d2));

  // one dep for deps/dep1, with the URL from the root's .pkg
  auto l = pkg::dependency_loader{deps};
  l.retrieve(dir);
  CHECK(l.get_all().size() == 3U);
  CHECK(l.resolve(url2).value()->url_ == url1);

  // terminates, checks out the latest commit and writes dep1 once
  pkg::load_deps(dir, deps, false, true, false);
  CHECK(pkg::get_commit(deps / "dep1") == c2);
  auto const cmake_lists = read_file(deps / "CMakeLists.txt");
  CHECK(cmake_lists.substr(cmake_lists.find("\n\n") + 2) ==
        "add_subdirectory(dep1 EXCLUDE_FROM_ALL)\n"
        "add_subdirectory(dep2 EXCLUDE_FROM_ALL)\n");
  auto const lock = read_file(dir / ".pkg.lock");
  CHECK(lock.substr(lock.find('\n') + 1) ==
        fmt::format("dep1 {}\ndep2 {}\n", c2, d2));

  fs::remove_all(dir);
}
