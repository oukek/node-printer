{
  "targets": [
    {
      "target_name": "printer",
      "sources": ["src/printer.cc"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "conditions": [
        ["OS=='mac'", {
          "libraries": ["-lcups"],
          "xcode_settings": {
            "OTHER_CPLUSPLUSFLAGS": ["-std=c++17"]
          }
        }],
        ["OS=='linux'", {
          "libraries": ["-lcups"]
        }],
        ["OS=='win'", {
          "libraries": ["winspool.lib", "gdi32.lib"]
        }]
      ]
    }
  ]
}
