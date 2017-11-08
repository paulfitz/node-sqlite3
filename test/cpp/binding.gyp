{
  "target_defaults":
    {
        "cflags" : ["-Wall", "-Wextra", "-Wno-unused-parameter"],
        "defines": [ "V8_DEPRECATION_WARNINGS=1" ],
        "include_dirs": ["<!(node -e \"require('../..')\")", "<!(node -e \"require('nan')\")"],
    },
  "targets": [
    {
        "target_name" : "marshal"
      , "sources"     : [ "marshal.cc" ]
    }
]}

