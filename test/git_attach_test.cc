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

  // master: a -> b, feature/a: a, dev: b
  pkg::exec(d.path_, "git init -q -b master");
  auto const a = commit("a");
  auto const b = commit("b");
  pkg::exec(d.path_, "git branch feature/a {}", a);
  pkg::exec(d.path_, "git branch dev {}", b);
  CHECK(pkg::as_branch(d.path_, a) == "feature/a");
  CHECK(pkg::as_branch(d.path_, b) == "master");

  auto e = pkg::executor{};

  // without force: checks out the branch at the commit
  d.commit_ = a;
  pkg::git_attach(e, &d, false);
  CHECK(pkg::get_commit(d.path_) == a);
  CHECK(head_branch() == "feature/a");

  // master is preferred when coming from another commit
  d.commit_ = b;
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "master");

  // a checked out branch at the commit stays
  pkg::exec(d.path_, "git checkout -q dev");
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "dev");

  // with force: detaches, the branches stay where they are
  d.commit_ = a;
  pkg::git_attach(e, &d, true);
  CHECK(pkg::get_commit(d.path_) == a);
  CHECK(head_branch() == "HEAD");
  CHECK(pkg::get_commit(d.path_, "master") == b);
  CHECK(pkg::get_commit(d.path_, "dev") == b);

  // no branch at the commit
  CHECK(!pkg::as_branch(d.path_, commit("c")).has_value());

  // remote branch heads, as after a fetch
  pkg::exec(d.path_, "git remote add origin {}", d.url_);
  auto const remote_head = [&](std::string const& from,
                               std::string const& branch) {
    pkg::exec(d.path_, "git checkout -q --detach {}", from);
    auto const c = commit(branch);
    pkg::exec(d.path_, "git update-ref refs/remotes/origin/{} {}", branch, c);
    pkg::exec(d.path_, "git checkout -q --detach {}", a);
    return c;
  };

  // master tracks origin/master and is behind: fast-forward
  d.commit_ = remote_head("master", "master");
  pkg::exec(d.path_, "git branch -q -u origin/master master");
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "master");
  CHECK(pkg::get_commit(d.path_, "master") == d.commit_);

  // no local branch for origin/feature/f: create it
  d.commit_ = remote_head(a, "feature/f");
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "feature/f");
  CHECK(pkg::get_commit(d.path_) == d.commit_);

  // dev tracks origin/dev and has diverged: detach, dev stays
  pkg::exec(d.path_, "git checkout -q dev");
  auto const local_dev = commit("local");
  d.commit_ = remote_head(b, "dev");
  pkg::exec(d.path_, "git branch -q -u origin/dev dev");
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "HEAD");
  CHECK(pkg::get_commit(d.path_) == d.commit_);
  CHECK(pkg::get_commit(d.path_, "dev") == local_dev);

  // release tracks fork/release, not origin/release: detach, release stays
  pkg::exec(d.path_, "git remote add fork git@example.com:fork/repo.git");
  pkg::exec(d.path_, "git update-ref refs/remotes/fork/release {}", a);
  pkg::exec(d.path_, "git branch -q --track release fork/release");
  d.commit_ = remote_head(a, "release");
  pkg::git_attach(e, &d, false);
  CHECK(head_branch() == "HEAD");
  CHECK(pkg::get_commit(d.path_, "release") == a);

  fs::remove_all(dir);
}
