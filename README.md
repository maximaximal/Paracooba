ParaCuber
=========

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
