pkg is a simple, minimal dependency manager based on Git.

It clones your cycle-free dependency tree recursively and creates a `CMakeLists.txt` file with all dependencies in topological order.

Example `.pkg` file:

    [fmt]
      url=git@github.com:motis-project/fmt.git
      branch=master
      commit=3eca62d66c7c9a2ca97dc8381299d911978b9fb2

## Useful commands
After invoking `CMake`, the `pkg` binary executable can be found in `build/dl/`.

### Displaying the dependency tree

    pkg -s

### Syncing deps/
After updating a commit ID in the `.pkg` file,
you can `pkg -l` to establish the dependency tree state described in the `.pkg` file.
Effectively, `git` repositories will be updated in `deps/`.

You can use `-f` to force checkout the commits.

> [!WARNING]
> This will override the changes you made, so be careful.
