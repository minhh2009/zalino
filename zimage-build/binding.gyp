{
  "targets": [
    {
      "target_name": "zimage",
      "sources": [
        "src/addon.cpp",
        "src/thumbnail.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-fexceptions"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "libraries": [
        "<!@(pkg-config --libs vips-cpp)"
      ],
      "cflags": [
        "<!@(pkg-config --cflags vips-cpp)"
      ]
    }
  ]
}
