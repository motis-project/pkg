#pragma once

#include <iostream>

#include "boost/filesystem/path.hpp"

#include "fmt/format.h"

namespace pkg {

void set_verbose(bool);

struct exec_result {
  friend std::ostream& operator<<(std::ostream&, exec_result const&);
  std::string to_str() const;
  boost::filesystem::path working_directory_;
  std::string command_;
  std::string out_, err_;
  int exit_code_;
};

struct exec_exception : public exec_result, std::runtime_error {
  exec_exception(exec_result&&);
};

exec_result exec(boost::filesystem::path const& working_directory,
                 std::string const& cmd);

template <typename... Args>
exec_result exec(boost::filesystem::path const& working_directory,
                 char const* command, Args... args) {
  return exec(working_directory, fmt::format(command, args...));
}

struct executor {
  template <typename... Args>
  exec_result exec(boost::filesystem::path const& working_directory,
                   char const* command, Args... args) {
    try {
      return results_.emplace_back(
          ::pkg::exec(working_directory, command, std::forward<Args>(args)...));
    } catch (exec_result const& e) {
      results_.emplace_back(e);
      throw;
    }
  }

  void print_trace() {
    if (!results_.empty()) {
      std::cout << "*** TRACE:\n";
      for (auto const& r : results_) {
        std::cout << r << "\n";
      }
    }
    std::cout << std::flush;
  }

  void clear() { results_.clear(); }

  std::vector<exec_result> results_;
};

}  // namespace pkg
