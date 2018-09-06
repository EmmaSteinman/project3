# Pintos Setup

This document describes the setup steps taken to get to the point of pintos-enabled running for both Ubuntu (on `hadoop2`), and on MacOS (on `bressoud3` and Tom's Macbook Pro)

These instructions are based on the ones given for the Johns Hopkins University CS318 Operating Systems class.

## Pintos clone

### MacOS

On Dropbox, I started with

```
git clone https://github.com/denison-cs/pintos.git
```
into `~/Dropbox/cs372-OpSys/pintos`

Note that this is a private repository, and actual distribution of source code is intended to occur through GitHub Classroom, through which each student will, at least initially, have their own repository with this as the "starter".

### Ubuntu

```
git clone https://github.com/denison-cs/pintos.git
```
into `~/cs372/pintos`

## Setup of Toolchain

From https://cs.jhu.edu/~huang/cs318/fall17/project/setup.html

### Ubuntu

#### Check

Did `objdump -i | grep elf32-i386` and got non-empty results, which the instructions say verify the support for the 32 bit toolchain, and concludes we can "skip this section"

#### Toolchain script based build

from clone directory
```
$ cd pintos/src/misc
$ ./toolchain-build.sh /path-to-cs372/toolchain
```

where, on Ubuntu, `/path-to-cs372` is `/users/bressoud/cs372`

And add cross-compiler executables to path:
```bash
export PATH=/users/bressoud/cs372/toolchain/x86_64/bin:$PATH
```
in `~\.bashrc`

### MacOS

#### Check

Did `objdump -i | grep elf32-i386` and got error return with -i not supported.  Look at man page did not see if there was some equivalent on the Mac platform.  

#### Brew

In order to get wget, and since, on `bressoud3`, there is not a version of Homebrew, I started with installing Homebrew:
```
ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
#### Toolchain script based build

from clone directory
```
$ cd pintos/src/misc
$ ./toolchain-build.sh /path-to-cs372/toolchain
```

where, on MacOS, `/path-to-cs372` is `/Users/bressoud/Dropbox/cs372-OpSys`

And add cross-compiler executables to path:
```bash
export PATH=/users/bressoud/cs372/toolchain/x86_64/bin:$PATH
```
in `~\.bash_profile`

## x86 Emulator

### Ubuntu

#### `qemu`
```
$ sudo apt-get install qemu libvirt-bin
```
Executable in `qemu-system-i386` is version 2.3.0, and executable in `qemu-i386` is version 2.5.0.

On checking machine `216a`, found that `qemu-system-i386` is aleady in path and is version 2.3.0, but is from a `~/local/bin` install, probably from Fall 2016.  Would recommend the apt-get above and not a local build from source.

#### `bochs` and `bochs-dbg`

Install prerequisites:
```
$ sudo apt-get install libx11-dev libxrandr-dev
```

Build `bochs` from 2.6.2 source using pintos build script

From clone directory
```
$ cd pintos/src/misc
$ ./bochs-2.6.2-build.sh /users/bressoud/cs372/toolchain/
```
This build created both `bin` and `share/bochs` directories under the `DSTDIR`, and the BIOS and VGABIOS images are under the latter.

And add executables to path:
```bash
export PATH=/users/bressoud/cs372/toolchain/bin:$PATH
```
in `~\.bashrc`

And
```bash
export BXSHARE=/users/bressoud/cs372/toolchain/share/bocks
```
in `~\.bashrc`, to set the environment variable so that bochs can find the images.

### MacOS

#### `qemu`
```
$ brew install qemu
```
Executable in `qemu-system-i386` and is version 2.12.0

#### `bochs` and `bochs-dbg`

Install prerequistes:  
1. XQuartz (version 2.7.11)

## Pintos Utility Tools

### Ubuntu

```bash
cd src/utils
make
cp backtrace pintos Pintos.pm pintos-gdb pintos-set-cmdline pintos-mkdisk setitimer-helper squish-pty squish-unix /users/bressoud/cs372/toolchain/bin
```

```bash
cd src/misc
mkdir /users/bressoud/cs372/toolchain/misc
cp gdb-macros /users/bressoud/cs372/toolchain/misc
```
## Others

### Ubuntu

#### Packages

- `ctags` (installs exuberant-ctags and links ctags)
- `cscope`
- `cgdb`
