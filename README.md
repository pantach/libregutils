*libregutils* is regex utilities library written in C, providing functions for matching, replacing (with added support for backreferences), splitting and escaping regular expressions. libregutils is built on top of, and requires a POSIX compatible regex library to be installed on your system (usually installed by default on UNIX systems).

## Features

- Easy interface
- Global and range searching, replacing and splitting
- Support for backreferences in the replacement string
- Supports basic (*BRE*) and extended (*ERE*) regular expressions
- Utilizing memory pools for added performance
- Complete error reporting
- Fully documented
- MIT license

## Quick Start

1. Download the github repository or a release version
2. Uncompress and open a terminal at the downloaded directory
3. Build and install by typing the following commands:
```console
autoreconf -i # Necessary only if you downloaded the repository
./configure
make
sudo make install
```
4. You can now use *libregutils* on you projects. Don't forget to link them by passing `-lregutils` to your compiler

See [examples](https://github.com/pantach/libregutils/tree/main/examples) for a demonstration of usage
