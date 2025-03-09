pkg is a simple, minimal dependency manager based on Git.

It clones your cycle-free dependency tree recursively and creates a `CMakeLists.txt` file with all dependencies in topological order.

Example `.pkg` file:

    [fmt]
      url=git@github.com:motis-project/fmt.git
      branch=master
      commit=3eca62d66c7c9a2ca97dc8381299d911978b9fb2

## Useful commands

When using [pkg.cmake](https://github.com/motis-project/motis/blob/master/cmake/pkg.cmake), the `pkg` binary executable
will be downloaded after invoking `cmake` if pkg is not already installed on the system. It can then be found in `build/dl/`.

### Displaying the dependency tree

    pkg -s

### Syncing deps/

After updating a commit ID in the `.pkg` file,
you can `pkg -l` to establish the dependency tree state described in the `.pkg` file.
Effectively, `git` repositories will be updated in `deps/`.

You can use `-f` to force checkout the commits.

> [!WARNING]
> This will override the changes you made, so be careful.

### Workflow

You can work on dependencies directly in the `deps` directory.
`pkg` will try to not get in your way while you are working on a dependency
by not changing the state of the dependency's checkout when

 * There are uncommitted changes on top of the commit specified in `.pkg`
 * There are unpushed commits on the currently checked out branch

Don't forget to update the dependency commit hash in `.pkg` to your new commit after you are done!
Otherwise `pkg` will try to check out the pinned commit.
