# Compiling

```sh
$ git clone git@github.com:orktes/max_r36s.git
$ cd max_r36s
$ cmake .
$ make build # Builds just the maxpayne_arm64 binary
$ make package # Creates a PortMaster compliant package in the `package/` directory. This also creates a proper port.json and README.md for the distribution.
$ make archive # Same as above but creates an archive under `archive/`
```

For convenience, it's recommended to use the devcontainer under `.devcontainer/`. If using VSCode (or other IDEs with support for [devcontainers](https://containers.dev/)), the IDE should automatically detect the container and offer to reopen the project inside it. This provides a consistent development environment and simplifies the setup process.

Alternatively, you can use the provided `./scripts/build_with_docker.sh` (run from root of the repo) script to build the project inside a Docker container. This script will handle all the necessary steps and dependencies for you.