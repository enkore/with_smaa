
# with_smaa

A Linux utility to use SMAA in games which do not support it by default.

Notice: proof of concept *only*

Current state of work:

- Works only with applications using OpenGL 3.0 or higher
- No options so far.
- Very crude support for multilib. Assumes 32 bit libraries are always at /usr/lib32/

## Installation

    mkdir build && cd build
	cmake ..
	make
	sudo make install

## Usage

    with_smaa path/to/game/executable [game options]
