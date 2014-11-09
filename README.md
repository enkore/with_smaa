
# with_smaa

A Linux utility to use SMAA in games which do not support it by default.

Current state of work:

- No support for resizing. Resizing currently causes massive corruptions.
- Requires GLSL 3.xx?
- Requires OpenGL 3.x/4.x
- Only works for GLX apps right now (majority uses GLX, including
  most/all? games available through Steam).

## Installation

    mkdir build && cd build
	cmake ..
	make
	sudo make install

## Usage

    with_smaa path/to/game/executable [game options]

## List of confirmed compatible applications

## List of confirmed INcompatible applications
