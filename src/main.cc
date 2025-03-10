#include <cstdio>
#include <algorithm>

#include "pkg/exec.h"
#include "pkg/load_deps.h"
#include "pkg/print_status.h"

namespace fs = boost::filesystem;
using namespace pkg;

extern bool verbose;

bool has_flag(int argc, char** argv, char const* str) {
  return std::any_of(argv, argv + argc,
                     [&](char* arg) { return std::strcmp(arg, str) == 0; });
}

int main(int argc, char** argv) {
  if (argc < 2 || has_flag(argc - 1, argv + 1, "--help") ||
      has_flag(argc - 1, argv + 1, "-h")) {
    printf(
        "Usage:\n"
        "  pkg load | -l      [clone dependencies]\n"
        "      -h          for https        (default: ssh);\n"
        "      -f          for hard reset   (default: checkout)]\n"
        "      -r          for recursive    (default: false)]\n"
        "      --drop-wip  check out ref from .pkg even though there are\n"
        "                  unpushed changes (default: false)\n"
        "  pkg status | -s    [print status]\n"
        "\n"
        "Common flags - apply everywhere:\n"
        "  -v        verbose [print what's going on]\n");
    return 0;
  }

  try {
    auto const mode = std::string_view{argv[1]};
    set_verbose(has_flag(argc - 1, argv + 1, "-v"));
    if (mode == "load" || mode == "-l") {
      load_deps(fs::path{"."}, fs::path("deps"),  //
                has_flag(argc - 1, argv + 1, "-h"),  //
                has_flag(argc - 1, argv + 1, "-f"),  //
                has_flag(argc - 1, argv + 1, "-r"),  //
                has_flag(argc - 1, argv + 1, "--drop-wip"));
    } else if (mode == "status" || mode == "-s") {
      print_status(fs::path{"."}, fs::path("deps"));
    } else {
      fmt::print("Unknown mode {}. See pkg --help for usage.", mode);
    }
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
