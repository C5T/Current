/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

var assert = require('assert');
var request = require('request');
var Claire = require('./claire.js');

const testKarlKeepalivesPort = 30001;
const testKarlFleetViewPort = 30999;

const claireConfig = {
  service: 'JS.Test',
  localPort: 30002,
  cloudInstanceName: 'i-12345',
  cloudAvailabilityGroup: 'g-1',
  karlURL: 'http://127.0.0.1:' + String(testKarlKeepalivesPort) + '/',
  keepaliveInterval: 10000
}

var customFieldValue = 0;

function statusFiller() {
  var status = { "custom_status": { custom_field: String(++customFieldValue) } };
  return status;
}

describe('Claire JS client test.', function () {
  var testKarlTerminated = false;
  var claire;

  it('Check that no service is running on Karl\'s port', function (done) {
    request({
      uri: claireConfig.karlURL + '/up'
    }, function (error, response) {
      if (error) {
        done();
      } else {
        console.error('Local port ' + String(testKarlKeepalivesPort) + ' is already used.');
        process.exit(1);
      }
    });
  });

  it('Start test Karl server', function (done) {
    require('child_process').spawn('.current/karl_server').on('close', function () {
      testKarlTerminated = true;
    });
    var retriesLeft = 10;
    var checkKarlIsUp = function () {
      request({
        uri: claireConfig.karlURL + 'up'
      }, function (error, response) {
        if (error || response.statusCode !== 200) {
          if (--retriesLeft) {
            setTimeout(checkKarlIsUp, 50);
          }
        } else {
          done();
        }
      });
    }
    checkKarlIsUp();
  });

  it('Spawn Claire instance and register in Karl', function (done) {
    claire = new Claire(claireConfig);
    claire.register(statusFiller);
    done();
  });

  it('Force Claire to send next keepalive', function (done) {
    claire.forceSendKeepalive();
    done();
  });

  it('Check that Karl responds with the proper fleet status', function (done) {
    const fleetViewURL = 'http://127.0.0.1:' + String(testKarlFleetViewPort) + '/';
    request({
      uri: fleetViewURL
    }, function (error, response) {
      if (error) {
        console.error('Can\'t get fleet state information from Karl.');
        process.exit(1);
      } else {
        assert.equal(response.statusCode, 200);
        var fleet = JSON.parse(response.body);
        assert.equal(fleet.machines['127.0.0.1'].cloud_instance_name, claireConfig.cloudInstanceName);
        assert.equal(fleet.machines['127.0.0.1'].cloud_availability_group, claireConfig.cloudAvailabilityGroup);
        assert(claire.codename in fleet.machines['127.0.0.1'].services);
        var service = fleet.machines['127.0.0.1'].services[claire.codename];
        assert("up" in service.currently);
        assert.equal(service.service, claireConfig.service);
        assert.equal(service.codename, claire.codename);
        assert.equal(service.location.ip, '127.0.0.1');
        assert.equal(service.location.port, claireConfig.localPort);
        assert.equal(service.location.prefix, '/');
        // There should be two keepalives sent at this moment.
        assert.equal(service.runtime.custom_status.custom_field, '2');
        done();
      }
    });
  });

  it('Deregister in Karl', function (done) {
    claire.deregister();
    done();
  });

  it('Check that Karl does not report our service as active', function (done) {
    const fleetViewURL = 'http://127.0.0.1:' + String(testKarlFleetViewPort) + '/?active_only';
    request({
      uri: fleetViewURL
    }, function (error, response) {
      if (error) {
        console.error('Can\'t get fleet state information from Karl.');
        process.exit(1);
      } else {
        assert.equal(response.statusCode, 200);
        var fleet = JSON.parse(response.body);
        assert.equal(Object.keys(fleet.machines).length, 0);
        done();
      }
    });
  });


  it('Request `/shutdown` for Karl server', function (done) {
    var retriesLeft = 10;
    var shutdownKarl = function () {
      request({
        uri: claireConfig.karlURL + 'shutdown'
      }, function (error, response) {
        if (error || response.statusCode !== 200) {
          if (--retriesLeft) {
            setTimeout(checkKarlIsUp, 50);
          }
        } else {
          done();
        }
      });
    }
    shutdownKarl();
  });

  it('Wait for Karl server to stop', function (done) {
    var wait = function () {
      if (!testKarlTerminated) {
        setTimeout(wait, 50);
      } else {
        done();
      }
    }
    wait();
  });
});
