#!/bin/sh

echo "Running build script from repository"
echo "(current dir is repo root: $PWD)"
cmake . && make
