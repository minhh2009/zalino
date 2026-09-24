#!/bin/bash
set -e
cd db-cross-build
npm install node-addon-api

npx node-gyp configure --target=22.3.27 --arch=x64 --dist-url=https://www.electronjs.org/headers build 
echo "[*] Success! Binary at: build/Release/db-cross-v4-native.node"
