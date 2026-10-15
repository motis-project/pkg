#include "pkg/git.h"

#include "boost/filesystem.hpp"

#include "utl/erase.h"
#include "utl/parser/cstr.h"

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

// Any form of a repository URL, with or without .git, becomes https://host/path
std::string normalize_repo_url(utl::cstr const url) {
  auto normalized = url_to_protocol(url.trim().to_str(), protocol::kHttps);
  if (normalized.ends_with(".git")) {
    normalized.resize(normalized.size() - 4);
  }
  return normalized;
}

std::string get_remote(executor& e, boost::filesystem::path const& p,
                       std::string const& url) {
  auto const normalized_url = normalize_repo_url(url);
  auto const remotes = e.exec(p, "git remote").out_;
  for (auto const line : utl::lines{remotes}) {
    auto const name = line.trim().to_str();
    auto const remote_url = e.exec(p, "git remote get-url {}", name).out_;
    if (normalize_repo_url(remote_url) == normalized_url) {
      return name;
    }
  }
  return "origin";
}

std::string get_commit(executor& e, boost::filesystem::path const& p,
                       std::string const& target = "HEAD") {
  auto out = exec(p, "git rev-parse {}", target).out_;
  utl::erase(out, '\n');
  return out;
}

std::optional<std::string> branch_at(executor& e,
                                     boost::filesystem::path const& p,
                                     std::string const& commit,
                                     std::string const& refs) {
  auto const out =
      e.exec(p, "git for-each-ref --points-at {} --format=%(refname) {}",
             commit, refs)
          .out_;
  auto branch = std::optional<std::string>{};
  for (auto const line : utl::lines{out}) {
    // refs/heads/feature/a -> feature/a
    auto const name = line.view().substr(refs.size() + 1);
    if (name == "master") {
      return "master";  // prefer master if commit is head of multiple branches
    }
    if (name != "HEAD" && !branch.has_value()) {
      branch = std::string{name};
    }
  }
  return branch;
}

std::optional<std::string> as_branch(boost::filesystem::path const& p,
                                     std::string const& commit) {
  auto e = executor{};
  return branch_at(e, p, commit, "refs/heads");
}

void checkout(executor& e, dep const* d) {
  if (auto const branch = branch_at(e, d->path_, d->commit_, "refs/heads")) {
    e.exec(d->path_, "git checkout {}", *branch);
    return;
  }

  auto const remote = get_remote(e, d->path_, d->url_);
  if (auto const branch =
          branch_at(e, d->path_, d->commit_, "refs/remotes/" + remote)) {
    auto const remote_branch = remote + "/" + *branch;

    // `:trackshort`
    // ">" (ahead)
    // "<" (behind)
    // "<>" (ahead and behind)
    // "=" (in sync)
    auto const local =
        e.exec(d->path_,
               "git for-each-ref "
               "--format=%(upstream)%(upstream:trackshort) refs/heads/{}",
               *branch)
            .out_;
    if (local.empty() || utl::cstr{local}.trim().view() ==
                             "refs/remotes/" + remote_branch + "<") {
      // No local branch -> set it up
      // or local branch is behind remote -> fast-forward
      e.exec(d->path_, "git checkout -B {} --track {}", *branch, remote_branch);
      return;
    }
  }

  e.exec(d->path_, "git checkout {}", d->commit_);
}

void git_attach(executor& e, dep const* d, bool const force) {
  if (get_commit(e, d->path_) == d->commit_) {
    return;
  }

  if (!commit_exists(d, d->commit_)) {
    e.exec(d->path_, "git fetch {}", get_remote(e, d->path_, d->url_));
  }

  if (force) {
    e.exec(d->path_, "git checkout --detach");
    e.exec(d->path_, "git reset --hard {}", d->commit_);
  } else {
    checkout(e, d);
  }

  if (boost::filesystem::exists(d->path_ / ".gitmodules")) {
    e.exec(d->path_, "git submodule sync");
    e.exec(d->path_, "git submodule update --init --recursive");
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
  checkout(e, d);
  e.exec(d->path_, "git submodule update --init --recursive");
}

std::string get_commit(boost::filesystem::path const& p,
                       std::string const& target) {
  auto e = executor{};
  return get_commit(e, p, target);
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

}  // namespace pkg
