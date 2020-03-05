#!/bin/bash

RELEASE_ID=$(git rev-parse --short HEAD)-$(date '+%Y%m%d')

rm -rf build-release
mkdir build-release

cd build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
make bcm-server -j $(nproc)
make offline-server -j $(nproc)

cd src
mv bcm-server bcm-server-${RELEASE_ID}
mv offline-server offline-server-${RELEASE_ID}

tar -czvf bcm-server-release-${RELEASE_ID}.tar.gz bcm-server-${RELEASE_ID} offline-server-${RELEASE_ID}

cd ../../
mv build-release/src/bcm-server-release-${RELEASE_ID}.tar.gz ./

rm -rf build-release
