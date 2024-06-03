#pragma once

#include <set>
#include <string>

#include "utl/struct/comparable.h"

#include "boost/filesystem/path.hpp"

#include "pkg/dep.h"
#include "pkg/exec.h"

namespace pkg {

struct commit_info {
  MAKE_COMPARABLE()
  std::string info_;
  branch_commit bc_;
};

std::string ssh_to_https(std::string url);

std::string git_shorten(dep const*, std::string const& commit);

void git_clone(executor&, dep const*, bool clone_https);

void git_attach(executor&, dep const*, bool force);

std::string get_commit(boost::filesystem::path const& p,
                       std::string const& target = "HEAD");

std::string commit(boost::filesystem::path const& p, std::string const& msg);

void push(boost::filesystem::path const& p);

std::vector<commit_info> get_commit_infos(
    boost::filesystem::path const& p, std::set<branch_commit> const& commits);

bool commit_exists(dep const*, std::string const& commit);

std::string commit_date(dep const*, std::string const& commit);

std::time_t commit_time(dep const*, std::string const& commit);

bool is_fast_forward(boost::filesystem::path const& p,
                     std::string const& commit1, std::string const& commit2);

}  // namespace pkg
