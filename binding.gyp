{
  "targets": [
    {
      "target_name": "printer",
      "sources": ["src/printer.cc"],
      "include_dirs": [
        "<!(node -p \"require('node-addon-api').include\")",
        "<!(pkg-config --cflags cups)"
      ],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "libraries": [
        "<!(pkg-config --libs cups)"
      ]
    }
  ]
}
