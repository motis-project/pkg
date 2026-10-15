#include "doctest.h"

#include <fstream>
#include <iterator>
#include <string>

#include "boost/filesystem.hpp"

#include "fmt/format.h"

#include "pkg/exec.h"
#include "pkg/load_deps.h"

namespace fs = boost::filesystem;

std::string read_file(fs::path const& p) {
  auto in = std::ifstream{p.generic_string()};
  return std::string{std::istreambuf_iterator<char>{in},
                     std::istreambuf_iterator<char>{}};
}

void write_file(fs::path const& p, std::string const& content) {
  std::ofstream{p.generic_string()} << content;
}

// A local repository with one commit, returns the commit.
std::string make_repo(fs::path const& p) {
  fs::create_directories(p);
  pkg::exec(p, "git init -q");
  write_file(p / "file", p.filename().string());
  pkg::exec(p, "git add file");
  pkg::exec(p,
            "git -c user.name=pkg -c user.email=pkg@example.com commit -q -m "
            "init");
  auto commit = pkg::exec(p, "git rev-parse HEAD").out_;
  while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r')) {
    commit.pop_back();
  }
  return commit;
}

std::string pkg_entry(std::string const& name, std::string const& commit) {
  return fmt::format(
      "[{0}]\n  url=git@example.com:pkg/{0}.git\n  branch=master\n  "
      "commit={1}\n",
      name, commit);
}

// deps/CMakeLists.txt follows the dependencies of .pkg, also when the deps
// folder holds more of them (another branch, a folder shared between builds)
// and every commit of .pkg.lock is checked out already.
TEST_CASE("cmake_lists_follow_lock") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto const deps = dir / "deps";
  auto const a = make_repo(deps / "a");
  auto const b = make_repo(deps / "b");
  auto const cmake_lists = deps / "CMakeLists.txt";

  // branch A: a
  write_file(dir / ".pkg", pkg_entry("a", a));
  pkg::load_deps(dir, deps, false, false, true);
  auto const lock_a = read_file(dir / ".pkg.lock");

  // branch B: a, b
  write_file(dir / ".pkg", pkg_entry("a", a) + pkg_entry("b", b));
  pkg::load_deps(dir, deps, false, false, true);
  CHECK(read_file(cmake_lists).find("add_subdirectory(b ") !=
        std::string::npos);

  // back to branch A (its .pkg and .pkg.lock), nothing to check out
  write_file(dir / ".pkg", pkg_entry("a", a));
  write_file(dir / ".pkg.lock", lock_a);
  pkg::load_deps(dir, deps, false, false, true);
  CHECK(read_file(cmake_lists).find("add_subdirectory(a ") !=
        std::string::npos);
  CHECK(read_file(cmake_lists).find("add_subdirectory(b ") ==
        std::string::npos);

  fs::remove_all(dir);
}
