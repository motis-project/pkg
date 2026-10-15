#include "doctest.h"

#include <fstream>
#include <iterator>
#include <string>

#include "boost/filesystem.hpp"

#include "fmt/format.h"

#include "pkg/exec.h"
#include "pkg/git.h"
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

std::string commit_all(fs::path const& p, std::string const& msg) {
  pkg::exec(p,
            "git -c user.name=pkg -c user.email=pkg@example.com commit -q -am "
            "{}",
            msg);
  return pkg::get_commit(p);
}

// -f restores the checkout of .pkg also when its commit is checked out:
// discards local changes and updates submodules (e.g. an interrupted run).
TEST_CASE("force_discards_local_changes") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto const deps = dir / "deps";
  auto const a = deps / "a";

  // a has the submodule sub at its second commit s2
  auto const sub = dir / "sub";
  auto const s1 = make_repo(sub);
  write_file(sub / "file", "s2");
  auto const s2 = commit_all(sub, "s2");
  make_repo(a);
  pkg::exec(a, "git -c protocol.file.allow=always submodule add -q {} sub",
            sub.generic_string());
  auto const commit = commit_all(a, "sub");
  write_file(dir / ".pkg", pkg_entry("a", commit));
  pkg::load_deps(dir, deps, false, true, false);

  // a local change, the submodule at its old commit
  write_file(a / "file", "changed");
  pkg::exec(a / "sub", "git checkout -q {}", s1);

  pkg::load_deps(dir, deps, false, false, false);
  CHECK(read_file(a / "file") == "changed");

  pkg::load_deps(dir, deps, false, true, false);
  CHECK(read_file(a / "file") == "a");
  CHECK(pkg::get_commit(a / "sub") == s2);
  CHECK(pkg::is_clean(a));

  fs::remove_all(dir);
}

// -f checks out the commit in a repository without a checked out commit
// (interrupted clone).
TEST_CASE("force_after_interrupted_clone") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto const deps = dir / "deps";
  auto const src = dir / "src" / "b";
  auto const commit = make_repo(src);
  fs::create_directories(deps / "b");
  pkg::exec(deps / "b", "git init -q");
  pkg::exec(deps / "b", "git fetch -q {} HEAD", src.generic_string());
  write_file(dir / ".pkg", pkg_entry("b", commit));

  pkg::load_deps(dir, deps, false, true, false);
  CHECK(pkg::get_commit(deps / "b") == commit);

  fs::remove_all(dir);
}

// No git command runs in a deps directory without its own repository: it
// would run in the repository around it (here the project, -f would reset it
// to the commit of the dependency).
TEST_CASE("no_git_without_repository") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto const deps = dir / "deps";
  auto const p1 = make_repo(dir);
  write_file(dir / "file", "p2");
  auto const p2 = commit_all(dir, "p2");
  pkg::exec(dir, "git remote add origin git@example.com:pkg/project.git");
  fs::create_directories(deps / "c");
  write_file(deps / "c" / "file", "no repository");
  write_file(dir / ".pkg", pkg_entry("c", p1));

  CHECK_THROWS(pkg::load_deps(dir, deps, false, true, false));
  CHECK(pkg::get_commit(dir) == p2);
  CHECK(read_file(dir / "file") == "p2");
  CHECK(pkg::exec(dir, "git remote get-url origin").out_ ==
        "git@example.com:pkg/project.git\n");

  fs::remove_all(dir);
}
