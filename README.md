ParaCuber
=========

External Dependencies
---------------------

This tool requires the following external dependencies. Please ensure that their
development headers are installed in order to be able to compile the software.

  - Boost Log
  - Boost Program Options
  - Optional: Boost Beast (required for embedded web-server)

Minimum tested boost version: 1.65.1

Building
--------

	# Build directory
    mkdir build
	cd build

	# Workaround for old CMake releases not having file(TOUCH ...)
	mkdir -p third_party/cadical-out/build/libcadical.a
    touch third_party/cadical/cadical-out/build/libcadical.a

	# Building
	cmake ..
	make -j4
