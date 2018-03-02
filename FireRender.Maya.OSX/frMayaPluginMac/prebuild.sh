#!/bin/sh

# clear old folders

pushd "/Users/Shared/RadeonProRender"
rm -rf "cache icons image lib plug-ins renderDesc scripts shaders shelves modules"
popd

mkdir -p "/Users/Shared/RadeonProRender"
