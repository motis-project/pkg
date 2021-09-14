#include "pkg/exec.h"

#include <sstream>

#include "boost/process/child.hpp"
#include "boost/process/io.hpp"
#include "boost/process/start_dir.hpp"

namespace pkg {

bool verbose = false;

void set_verbose(bool const b) { verbose = b; }

std::ostream& operator<<(std::ostream& out, exec_result const& r) {
  return out << fmt::format(
             "COMMAND=\"{}\":\n"
             "  WORKING_DIRECTORY=\"{}\"\n"
             "  EXIT_CODE {}\n"
             "  OUTPUT: {}"
             "  ERROR: {}\n",
             r.command_, r.working_directory_.string(), r.exit_code_, r.out_,
             r.err_);
}

std::string exec_result::to_str() const {
  std::stringstream ss;
  ss << *this;
  return ss.str();
}

exec_exception::exec_exception(exec_result&& r)
    : exec_result{r}, std::runtime_error{r.to_str()} {}

exec_result exec(boost::filesystem::path const& working_directory,
                 std::string const& cmd) {
  namespace bp = boost::process;
  std::stringstream out_ss, err_ss;

  bp::ipstream out, err;
  bp::child c(cmd, bp::start_dir = working_directory, bp::std_out > out,
              bp::std_err > err);

  std::string line;
  while (true) {
    if (!out && !err) {
      break;
    }

    if (out) {
      std::getline(out, line);
      if (!line.empty()) {
        out_ss << line << "\n";
        continue;
      }
    }

    if (err) {
      std::getline(err, line);
      if (!line.empty()) {
        err_ss << line << "\n";
        continue;
      }
    }
  }

  c.wait();

  exec_result r;
  r.command_ = cmd;
  r.working_directory_ = working_directory;
  r.out_ = out_ss.str();
  r.err_ = err_ss.str();
  r.exit_code_ = c.exit_code();

  if (c.exit_code() != 0) {
    throw exec_exception{std::move(r)};
  } else {
    return r;
  }
}

}  // namespace pkg
