#include <cstdio>
#include <algorithm>
#include <span>

#include "pkg/exec.h"
#include "pkg/load_deps.h"
#include "pkg/print_status.h"

namespace fs = boost::filesystem;
using namespace pkg;

extern bool verbose;

bool has_flag(const std::span<char*> args, char const* str) {
  return std::any_of(args.begin(), args.end(),
                     [&](char* arg) { return std::strcmp(arg, str) == 0; });
}

int main(int argc, char** argv) {
  auto const args = std::span(argv + 1, argc - 1);
  if (argc < 2 || has_flag(args, "--help")) {
    printf(
        "Usage:\n"
        "  pkg load | -l      [clone dependencies]\n"
        "      -h          for https        (default: ssh)\n"
        "      -f          for hard reset   (default: checkout)\n"
        "      -r          for recursive    (default: false)\n"
        "      --drop-wip  check out ref from .pkg even though there are\n"
        "                  unpushed changes (default: false)\n"
        "      --ssh       Clone using ssh  (default: https)\n"
        "  pkg status | -s    [print status]\n"
        "\n"
        "Common flags - apply everywhere:\n"
        "  -v        verbose [print what's going on]\n");
    return 0;
  }

  const auto https = has_flag(args, "-h");
  const auto ssh = has_flag(args, "--ssh");
  if (https && ssh) {
    fmt::println(stderr, "-h and --ssh cannot be used at the same time.");
    return 1;
  }

  try {
    auto const mode = std::string_view{argv[1]};
    set_verbose(has_flag(args, "-v"));
    if (mode == "load" || mode == "-l") {
      load_deps(fs::path{"."}, fs::path("deps"),  //
                !ssh,  //
                has_flag(args, "-f"),  //
                has_flag(args, "-r"),  //
                has_flag(args, "--drop-wip"));
    } else if (mode == "status" || mode == "-s") {
      print_status(fs::path{"."}, fs::path("deps"));
    } else {
      fmt::println(stderr, "Unknown mode {}. See pkg --help for usage.", mode);
    }
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
