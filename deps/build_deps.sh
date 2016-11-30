#!/bin/bash
START_DIR=$PWD
tar xf ./deps/jansson-2.9.tar.gz
cd jansson-2.9
./configure --prefix=$START_DIR/bdeps/
make -j16
make install
rm -fr ${START_DIR}/jansson-2.9

