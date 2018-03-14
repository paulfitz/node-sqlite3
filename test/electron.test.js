var assert = require('assert');

describe('electron', function() {
  it('respects ELECTRON_VERSION', function() {
    process.env.ELECTRON_VERSION = '1.2.3';
    assert.throws(function() { require('..'); },
                  (/Cannot find module .*\/node-sqlite3\/lib\/binding\/electron-v1.2-[^-]+-x64\/node_sqlite3.node/));
  });
});
