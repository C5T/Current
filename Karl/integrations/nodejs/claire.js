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

var request = require('request');

function timeIntervalAsHumanReadableString(begin, end) {
  if (end < begin) {
    return '-' + timeIntervalAsHumanReadableString(end, begin);
  }

  var seconds = Math.floor((end - begin) / 1000);
  if (seconds < 60) {
    return String(seconds) + 's';
  } else {
    var minutes = Math.floor(seconds / 60);
    seconds %= 60;
    if (minutes < 60) {
      return String(minutes) + 'm ' + String(seconds) + 's';
    } else {
      var hours = Math.floor(minutes / 60);
      minutes %= 60;
      if (hours < 24) {
        return String(hours) + 'h ' + String(minutes) + 'm ' + String(seconds) + 's';
      } else {
        var days = Math.floor(hours / 24);
        hours %= 24;
        return String(days) + 'd ' + String(hours) + 'h ' + String(minutes) + 'm ' + String(seconds) + 's';
      }
    }
  }
}

function generateCodename () {
  var codename = '';
  for (var i = 0; i < 6; i++) {
    codename += String.fromCharCode(65 + Math.floor(Math.random() * 26));
  }
  return codename;
}

function Claire (config) {
  const startTimestamp = Date.now();
  const codename = generateCodename();
  var claireStatus_;
  var statusFiller_;
  var lastKeepaliveAttemptResult_ = { timestamp: null, status: null };
  var registered_ = false;
  var keepaliveTimeoutId_;
  var lastSuccessfulKeepaliveTimestamp_;
  var lastSuccessfulKeepalivePing_;

  Object.defineProperty(this, 'config', { value: config, writable: false });
  Object.defineProperty(this, 'codename', { value: codename, writable: false });
  Object.defineProperty(this, 'startTimestamp', { value: startTimestamp, writable: false });
  Object.defineProperty(this,
                        'keepaliveInterval',
                        {
                          value: (config.keepaliveInterval !== undefined ? config.keepaliveInterval : 20000),
                          writable: false
                        });

  this.setStatusFiller = function (value) { statusFiller_ = value; }
  this.isRegistered = function () { return registered_; }
  this.setRegistered = function (value) { registered_ = value; }

  this.keepaliveLoop = function () {
    if (registered_) {
      this.fillKeepaliveStatus();
      this.sendKeepalive();
      keepaliveTimeoutId_ = setTimeout(this.keepaliveLoop.bind(this), this.keepaliveInterval);
    }
  };

  this.clearKeepaliveTimeout = function () {
    if (keepaliveTimeoutId_ !== undefined && keepaliveTimeoutId_) {
      clearTimeout(keepaliveTimeoutId_);
      keepaliveTimeoutId_ = null;
    }
  };

  this.fillKeepaliveStatus = function () {
    var now = Date.now();
    var dependencies = (config.dependencies !== undefined && Array.isArray(config.dependencies) ?
        config.dependencies : []);
    var lastKeepaliveSent = (lastKeepaliveAttemptResult_.timestamp !== null ?
        timeIntervalAsHumanReadableString(lastKeepaliveAttemptResult_.timestamp, now) + ' ago' : '');
    var lastKeepaliveStatus = (lastKeepaliveAttemptResult_.status !== null ?
        lastKeepaliveAttemptResult_.status : '');
    var lastSuccessfulKeepalive = (lastSuccessfulKeepaliveTimestamp_ !== undefined ?
        timeIntervalAsHumanReadableString(lastSuccessfulKeepaliveTimestamp_, now) + ' ago' : undefined);
    var lastSuccessfulKeepalivePing = (lastSuccessfulKeepalivePing_ !== undefined ?
        String(lastSuccessfulKeepalivePing_) + 'ms' : undefined);
    var lastSuccessfulKeepalivePingUs = (lastSuccessfulKeepalivePing_ !== undefined ?
        lastSuccessfulKeepalivePing_ * 1000 : undefined);
    claireStatus = {
      service: config.service,
      codename: codename,
      local_port: config.localPort,
      cloud_instance_name: config.cloudInstanceName,
      cloud_availability_group: config.cloudAvailabilityGroup,
      dependencies: dependencies,
      reporting_to: config.karlUrl,
      now: now * 1000,
      start_time_epoch_microseconds: startTimestamp * 1000,
      uptime: timeIntervalAsHumanReadableString(startTimestamp, now),
      last_keepalive_sent: lastKeepaliveSent,
      last_keepalive_status: lastKeepaliveStatus,
      last_successful_keepalive: lastSuccessfulKeepalive,
      last_successful_keepalive_ping: lastSuccessfulKeepalivePing,
      last_successful_keepalive_ping_us: lastSuccessfulKeepalivePingUs,
      build: config.buildInfo,
    };

    if (statusFiller_ !== undefined && statusFiller_) {
      claireStatus.runtime = statusFiller_();
    }
  };

  this.sendKeepalive = function () {
    var beforeRequestTimestamp = Date.now();

    request({
      uri: this.config.karlUrl,
      method: 'POST',
      qs: { codename: this.codename, port: this.config.localPort },
      json: true,
      body: claireStatus
    }, function (error, response) {
      var afterRequestTimestamp = Date.now();
      lastKeepaliveAttemptResult_.timestamp = afterRequestTimestamp;
      if (error) {
        lastKeepaliveAttemptResult_.status = 'HTTP connection attempt failed';
      } else if (response.statusCode < 200 || response.statusCode > 299) {
        lastKeepaliveAttemptResult_.status = 'HTTP response code ' + String(response.statusCode);
      } else {
        lastKeepaliveAttemptResult_.status = 'Success';
        lastSuccessfulKeepaliveTimestamp_ = afterRequestTimestamp;
        lastSuccessfulKeepalivePing_ = afterRequestTimestamp - beforeRequestTimestamp;
      }
    });
  };

}

Claire.prototype.constructor = Claire;

// TODO(mzhurovich): Strict register.
Claire.prototype.register = function (statusFiller) {
  if (!this.isRegistered()) {
    this.setStatusFiller(statusFiller);
    this.setRegistered(true);
    this.keepaliveLoop();
  }
};

Claire.prototype.deregister = function (callback) {
  if (this.isRegistered()) {
    this.clearKeepaliveTimeout();
    this.setRegistered(false);
    request({
      uri: this.config.karlUrl,
      method: 'DELETE',
      qs: { codename: this.codename }
    }, function (error, response) {
      if (typeof callback === 'function') {
        if (error) {
          callback({ success: false, error: 'HTTP connection attempt failed' });
        } else if (response.statusCode < 200 || response.statusCode > 299) {
          callback({ success: false, error: 'HTTP request error', response: response });
        } else {
          callback({ success: true });
        }
      }
    });
  }
};

Claire.prototype.forceSendKeepalive = function () {
  if (this.isRegistered()) {
    // Clear existing keepalive timeout and restart the loop.
    this.clearKeepaliveTimeout();
    this.keepaliveLoop();
  }
};

module.exports = Claire;
