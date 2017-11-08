/* global describe, it, escape */

const path = require('path');
const assert = require('assert');
const util = require('util');
const bindings = require('bindings');

const testRoot = path.resolve(__dirname, 'cpp');
const mainRoot = path.resolve(__dirname, '..');
bindings({ module_root: mainRoot, bindings: 'node_sqlite3' });
const marshal = bindings({ module_root: testRoot, bindings: 'marshal' });

describe('marshal', function() {
  function stringToArray(str) {
    return new Uint8Array(new Buffer(str));
  }
  const samples = [
    [null, 'N'],
    [1, 'i\x01\x00\x00\x00'],
    [1000000, 'i@B\x0f\x00'],
    [-123456, 'i\xc0\x1d\xfe\xff'],
    [1.23, 'g\xae\x47\xe1\x7a\x14\xae\xf3\x3f'],
    [-625e-4, 'g\x00\x00\x00\x00\x00\x00\xb0\xbf'],
    [true, 'T'],
    [false, 'F'],
    [stringToArray('Hello world'), 's\x0b\x00\x00\x00Hello world'],
    ['Résumé', 'u\x08\x00\x00\x00R\xc3\xa9sum\xc3\xa9'],
    [[1, 2, 3],
      '[\x03\x00\x00\x00i\x01\x00\x00\x00i\x02\x00\x00\x00i\x03\x00\x00\x00'],
    [{'This': 4, 'is': 0, 'a': stringToArray('test')},
      '{u\x04\x00\x00\x00Thisi\x04\x00\x00\x00u\x01\x00\x00\x00as\x04\x00\x00\x00testu\x02\x00\x00\x00isi\x00\x00\x00\x000'],
  ];

  it("should serialize correctly", function() {
    for (const [value, expectedAsString] of samples) {
      const expected = binStringToArray(expectedAsString);
      const marshalled = marshal.serialize(value);
      assert.deepEqual(marshalled, expected,
                       "Wrong serialization of " + util.inspect(value) +
                       "\n        actual: " + escape(arrayToBinString(marshalled)) +
                       "\n      expected: " + escape(arrayToBinString(expected)));
    }
  });
});


function binStringToArray(binaryString) {
  var a = new Uint8Array(binaryString.length);
  for (var i = 0; i < binaryString.length; i++) {
    a[i] = binaryString.charCodeAt(i);
  }
  return a;
}

function arrayToBinString(array) {
  return String.fromCharCode.apply(String, array);
}
