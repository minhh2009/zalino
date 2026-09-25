#!/bin/bash
set -e

TARGET="${1:-22.3.27}"
ARCH="${2:-x64}"
cd native/zjxl
npm install node-addon-api

npx node-gyp configure --target=$TARGET --arch=$ARCH --dist-url=https://www.electronjs.org/headers build 
echo "[*] Success! Binary at: build/Release/zjxl.node"