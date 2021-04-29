{
  "target_defaults":
    {
        "cflags" : ["-Wall", "-Wextra", "-Wno-unused-parameter"],
        "defines": [ "V8_DEPRECATION_WARNINGS=1" ],
        "conditions" : [
            ["OS=='linux'", {"libraries+": ["../../../build/<(PRODUCT_DIR)/node_sqlite3.node"] } ]
        ],
        "include_dirs": [
          "<!@(node -p \"require('node-addon-api').include\")","<!(node -e \"require('../..')\")"],
    },
  "targets": [
    {
        "target_name" : "marshal",
        "sources"     : [ "marshal.cc" ],
        "defines": [ "NAPI_VERSION=<(napi_build_version)", "NAPI_DISABLE_CPP_EXCEPTIONS=1" ]
    }
]}

