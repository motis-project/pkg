#include "pkg/git.h"

#include <algorithm>
#include <iostream>

#include "boost/filesystem.hpp"

#include "utl/erase.h"
#include "utl/parser/cstr.h"
#include "utl/to_vec.h"

#include "pkg/exec.h"

namespace pkg {

std::string normalize_url(std::string url) {
  if (url.starts_with("ssh://") || url.starts_with("https://") ||
      url.starts_with("http://")) {
    return url;
  }

  auto const at_pos = url.find("@");
  auto const colon_pos = url.find(":");

  // Try to detect urls in the form username@ssh-server:path
  if (at_pos != std::string::npos && colon_pos != std::string::npos &&
      at_pos < colon_pos) {
    // Replace ":" -> "/"
    url[colon_pos] = '/';

    // Replace git@ -> https://
    return "ssh://" + url;
  }

  // Give up and hope for the best
  return url;
}

std::string url_to_protocol(std::string url, protocol const p) {
  auto const normalized_url = normalize_url(url);

  auto const protocol_len = normalized_url.find("://");
  auto const without_protocol = normalized_url.substr(protocol_len + 3);

  switch (p) {
    case protocol::kHttps: {
      auto const username_pos = without_protocol.find('@');
      if (username_pos != std::string::npos) {
        return "https://" + without_protocol.substr(username_pos + 1);
      } else {
        return "https://" + without_protocol;
      }
    }
    case protocol::kSsh: {
      if (without_protocol.contains('@')) {
        return "ssh://" + without_protocol;
      } else {
        return "ssh://git@" + without_protocol;
      }
    }
  }

  return url;
}

std::string git_shorten(dep const* d, std::string const& commit) {
  auto out = exec(d->path_, "git rev-parse --short {}", commit).out_;
  utl::erase(out, '\n');
  return out;
}

std::string get_commit(executor& e, boost::filesystem::path const& p,
                       std::string const& target = "HEAD") {
  auto out = exec(p, "git rev-parse {}", target).out_;
  utl::erase(out, '\n');
  return out;
}

void git_attach(executor& e, dep const* d, bool const force) {
  if (get_commit(e, d->path_) == d->commit_) {
    return;
  }

  if (!commit_exists(d, d->commit_)) {
    e.exec(d->path_, "git fetch origin");
    e.exec(d->path_, "git checkout -B {} origin/{}", d->branch_, d->branch_);
  }

  std::string branch_head_commit;
  try {
    branch_head_commit = get_commit(e, d->path_, d->branch_);
  } catch (std::exception const& e) {
    fmt::print("warning: unknown branch {} for dependency {}\n", d->branch_,
               d->name());
    std::cout << std::flush;
  }
  auto const is_branch_head = branch_head_commit == d->commit_;
  auto const ref = is_branch_head ? d->branch_ : d->commit_;
  force ? e.exec(d->path_, "git reset --hard {}", ref)
        : e.exec(d->path_, "git checkout {}", ref);

  if (boost::filesystem::exists(d->path_ / ".gitmodules")) {
    e.exec(d->path_, "git submodule sync");
    e.exec(d->path_, "git submodule update --init --recursive --remote");
  }
}

void git_clone(executor& e, dep const* d, bool const to_https) {
  if (!boost::filesystem::is_directory(d->path_.parent_path())) {
    boost::filesystem::create_directories(d->path_.parent_path());
  }

  e.exec(d->path_.parent_path(), "git clone {} {}",
         url_to_protocol(d->url_, to_https ? protocol::kHttps : protocol::kSsh),
         boost::filesystem::absolute(d->path_).string());
  if (to_https) {
    e.exec(d->path_, "git remote set-url --push origin {}",
           url_to_protocol(d->url_, protocol::kSsh));
  }
  e.exec(d->path_, "git checkout {}", d->commit_);
  e.exec(d->path_, "git submodule update --init --recursive");
}

void git_attach(dep const* d, bool const force) {
  auto e = executor{};
  git_attach(e, d, force);
}

std::string get_commit(boost::filesystem::path const& p,
                       std::string const& target) {
  auto e = executor{};
  return get_commit(e, p, target);
}

std::string commit(boost::filesystem::path const& p, std::string const& msg) {
  constexpr auto const PKG_FILE = ".pkg";
  exec(p, "git add {}", PKG_FILE);
  exec(p, "git commit -m \"{}\"", msg);
  return get_commit(p);
}

void push(boost::filesystem::path const& p) { exec(p, "git push"); }

std::vector<commit_info> get_commit_infos(
    boost::filesystem::path const& p, std::set<branch_commit> const& commits) {
  auto infos = utl::to_vec(commits, [&](auto&& bc) -> commit_info {
    auto info = exec(p, "git show -s --format=%at {}", bc.commit_).out_;
    info.resize(info.size() - 1);
    return {info, bc};
  });
  std::sort(begin(infos), end(infos), std::greater<>());
  return infos;
}

bool commit_exists(dep const* d, std::string const& commit) {
  try {
    auto info = exec(d->path_, "git cat-file -t {}", commit).out_;
    info.resize(info.size() - 1);
    return info == "commit";
  } catch (...) {
    return false;
  }
}

std::string commit_date(dep const* d, std::string const& commit) {
  auto info =
      exec(d->path_, "git show -s --date=local --format=%cd {}", commit).out_;
  info.resize(info.size() - 1);
  return info;
}

std::time_t commit_time(dep const* d, std::string const& commit) {
  auto info =
      exec(d->path_, "git show -s --date=local --format=%at {}", commit).out_;
  info.resize(info.size() - 1);
  return std::stol(info);
}

bool is_fast_forward(boost::filesystem::path const& p,
                     std::string const& commit1, std::string const& commit2) {
  return exec(p, "git merge-base --is-ancestor {} {}", commit1, commit2)
             .exit_code_ == 0;
}

}  // namespace pkg
