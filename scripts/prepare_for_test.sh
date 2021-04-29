#!/bin/bash

set -e

bindings_module_likes=$(echo "console.log('node-v' + process.versions.modules + '-' + process.platform + '-' + process.arch)" | node)
cd lib/binding
target=$(find . -iname "node_sqlite3.node" | grep -v $bindings_module_likes | head -n1)
mkdir -p $bindings_module_likes
ln -s ../$target $bindings_module_likes/node_sqlite3.node || echo ok
