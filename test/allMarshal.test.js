/* globals describe, it, before, after */
var sqlite3 = require('..');
var assert = require('assert');

describe('Database#allMarshal', function() {
    var db;
    before(function(done) { db = new sqlite3.Database(':memory:', done); });

    var testValues = [0x7FFFFFFF, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER + 1];
    var marshalled = ['i\xff\xff\xff\x7f', 'g\xff\xff\xff\xff\xff\xff?C', 'g\x00\x00\x00\x00\x00\x00@C'];
    var marshalledFloat = ['g\x00\x00\xc0\xff\xff\xff\xdfA', marshalled[1], marshalled[2]];

    it('should create the table', function(done) {
        db.run("CREATE TABLE foo (row text, num int, flt float, blb blob)", function(err) {
            if (err) throw err;
            var inserted = 0;
            for (var i = 0; i < testValues.length; i++) {
                var val = testValues[i];
                db.run("INSERT INTO foo VALUES(?, ?, ?, ?)",
                    'row' + i, val, val, val,
                    function(err) {
                        if (err) throw err;
                        inserted++;
                        if (inserted === testValues.length) {
                            done();
                        }
                    }
                );
            }
        });
    });

    it('should retrieve all rows', function(done) {
        var fields = ['num', 'flt', 'blb'];
        var count = 0;
        var results = [];
        testValues.forEach(function(value, i) {
            results[i] = {};
            fields.forEach(function(field) {
                count++;
                db.allMarshal("SELECT " + field + " as f FROM foo WHERE row=\"row" + i + "\"",
                  function(err, result) {
                      results[i][field] = result;
                      if (--count === 0) { compare(); }
                  }
                );
            });
        });
        function compare() {
            testValues.forEach(function(value, i) {
                fields.forEach(function(field) {
                    // We query in a way so that marshalled data has the same form for all values:
                    var mvalue = (field === 'flt') ? marshalledFloat[i] : marshalled[i];
                    var expect = Buffer.from('{s\x01\x00\x00\x00f[\x01\x00\x00\x00' + mvalue + '0', 'binary');
                    var result = results[i][field];
                    assert.deepEqual(result, expect);
                });
            });
            done();
        }
    });

    after(function(done) { db.close(done); });
});
