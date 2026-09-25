{
  "targets": [
    {
      "target_name": "zjxl",

      "sources": [
        "src/zjxl.cpp"
      ],

      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],

      "defines": [
        "NAPI_CPP_EXCEPTIONS",
        "NAPI_VERSION=8"
      ],

      "cflags_cc": [
        "-std=c++17",
        "-fexceptions",
        "-Wall",
        "-Wextra",
        "-Wpedantic"
      ],

      "cflags": [
        "-fexceptions"
      ],

      "libraries": [
        "-ljxl",
        "-ljxl_threads"
      ],

      "conditions": [
        [
          "OS=='linux'",
          {
            "cflags_cc": [
              "-std=c++17",
              "-fexceptions",
              "-fPIC"
            ]
          }
        ]
      ]
    }
  ]
}