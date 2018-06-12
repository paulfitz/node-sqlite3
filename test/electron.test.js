var assert = require('assert');

describe('electron', function() {
  it('respects ELECTRON_VERSION', function() {
    process.env.ELECTRON_VERSION = '1.2.3';
    let name = require.resolve('..');
    delete require.cache[name];

    assert.throws(function() { require('..'); },
                  (/Cannot find module .*\/lib\/binding\/electron-v1.2-[^-]+-x64\/node_sqlite3.node/),
                  "Should have error like 'cannot find module'");
  });
});
