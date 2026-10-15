#include "doctest.h"

#include <string>

#include "boost/filesystem.hpp"

#include "pkg/dep.h"
#include "pkg/exec.h"
#include "pkg/git.h"

namespace fs = boost::filesystem;

TEST_CASE("git_attach") {
  auto const dir =
      fs::temp_directory_path() / fs::unique_path("pkg-test-%%%%-%%%%");
  auto d = pkg::dep{dir, "git@example.com:pkg/repo.git", ""};
  fs::create_directories(d.path_);

  auto const commit = [&](std::string const& msg) {
    pkg::exec(d.path_,
              "git -c user.name=pkg -c user.email=pkg@example.com commit -q "
              "--allow-empty -m {}",
              msg);
    return pkg::get_commit(d.path_);
  };
  auto const head_branch = [&]() {
    auto out = pkg::exec(d.path_, "git rev-parse --abbrev-ref HEAD").out_;
    return out.substr(0, out.find('\n'));
  };

  // master: a -> b, feature/a: a
  pkg::exec(d.path_, "git init -q -b master");
  auto const a = commit("a");
  auto const b = commit("b");
  pkg::exec(d.path_, "git branch feature/a {}", a);
  CHECK(pkg::as_branch(d.path_, a) == "feature/a");
  CHECK(pkg::as_branch(d.path_, b) == "master");

  auto e = pkg::executor{};
  d.commit_ = a;

  // without force: checks out the branch at the commit
  pkg::git_attach(e, &d, false);
  CHECK(pkg::get_commit(d.path_) == a);
  CHECK(head_branch() == "feature/a");

  // with force: detaches, the branches stay where they are
  pkg::exec(d.path_, "git checkout -q master");
  pkg::git_attach(e, &d, true);
  CHECK(pkg::get_commit(d.path_) == a);
  CHECK(head_branch() == "HEAD");
  CHECK(pkg::get_commit(d.path_, "master") == b);

  // no branch at the commit
  CHECK(!pkg::as_branch(d.path_, commit("c")).has_value());

  fs::remove_all(dir);
}
