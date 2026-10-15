#pragma once

#include <optional>
#include <string>

#include "boost/filesystem/path.hpp"

#include "pkg/dep.h"
#include "pkg/exec.h"

namespace pkg {

enum protocol { kSsh, kHttps };

std::string url_to_protocol(std::string url, protocol);

std::string git_shorten(dep const*, std::string const& commit);

void git_clone(executor&, dep const*, bool clone_https);

std::optional<std::string> as_branch(boost::filesystem::path const&,
                                     std::string const&);

void git_attach(executor&, dep const*, bool force);

std::string get_remote(executor&, boost::filesystem::path const& p,
                       std::string const& url);

std::string get_commit(boost::filesystem::path const& p,
                       std::string const& target = "HEAD");

bool commit_exists(dep const*, std::string const& commit);

std::string commit_date(dep const*, std::string const& commit);

std::time_t commit_time(dep const*, std::string const& commit);

}  // namespace pkg
