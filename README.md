# Enigmatic

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Requirements](#requirements)
4. [Installation](#installation)
5. [Usage](#usage)
6. [Examples](#examples)
7. [Contributing](#contributing)
8. [License](#license)

## Overview

**Disk streaming, system monitoring, and querying toolkit**

Enigmatic is a powerful toolkit that provides disk streaming, system monitoring, logging, and querying capabilities. Designed with developers in mind, it features an event-driven client API and an efficient daemon for resource tracking.

The Enigmatic daemon streams data to disk, creating a history that can be played back for analysis. Utilizing **LZ4 compression**, it efficiently compresses data blocks and further optimizes historical log files upon rotation.

## Features

- **Efficient Logging:** Tracks CPU cores every 1/10 second and other system resources every second.
- **On-Demand Polling:** The daemon's polling frequency can be dynamically adjusted via IPC.
- **Cross-Platform:** Tested on Linux, FreeBSD, and OpenBSD, including unique hardware configurations.
- **Developer-Friendly:** Writing custom client tools is straightforward with the provided event-driven API.

## Requirements

Before installing Enigmatic, ensure you have the following dependencies installed:

- **EFL (Enlightenment Foundation Libraries)**
- **Meson build system**
- **Ninja build tool**
- **C compiler (e.g., GCC or Clang)**

On Debian/Ubuntu-based systems, you can install dependencies with:

```sh
$ sudo apt install libefl-dev meson ninja-build gcc
```

On Fedora-based systems:

```sh
$ sudo dnf install efl-devel meson ninja-build gcc
```

## Installation

To install Enigmatic, clone the repository and follow the build instructions using Meson and Ninja:

```sh
$ git clone https://github.com/haxworx/enigmatic
$ cd enigmatic
$ meson setup build
$ ninja -C build
$ sudo ninja -C build install
```

## Usage

Starting the Enigmatic daemon:

```sh
$ enigmatic
```

Stopping the daemon:

```sh
$ enigmatic -s
```

## Examples

Enigmatic provides several example client applications to visualize and interact with system data:

1. **enigmatic_client:** Reference client demonstrating core features.
2. **memories:** EFL-based memory visualization tool.
3. **cpeew:** CPU resource visualizer.
4. **blindmin:** System administration tool designed for visually impaired users.

Explore the `examples` directory for detailed usage.

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests via our GitHub repository.

## License

Enigmatic is released under the MIT License. See the LICENSE file for details.
