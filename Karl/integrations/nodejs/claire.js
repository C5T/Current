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
  this.lastKeepaliveAttemptResult_ = { timestamp: null, status: null };
  this.registered_ = false;

  Object.defineProperty(this, 'config', { value: config, writable: false });
  Object.defineProperty(this, 'codename', { value: codename, writable: false });
  Object.defineProperty(this, 'startTimestamp', { value: startTimestamp, writable: false });
  Object.defineProperty(this,
                        'keepaliveInterval',
                        {
                          value: (config.keepaliveInterval !== undefined ? config.keepaliveInterval : 20000),
                          writable: false
                        });

}

Claire.prototype.constructor = Claire;

// TODO(mzhurovich): Strict register.
Claire.prototype.register = function (statusFiller) {
  if (!this.registered_) {
    if (typeof statusFiller === 'function') {
      this.statusFiller_ = statusFiller;
    }
    this.registered_ = true;
    this.keepaliveLoop_();
  }
};

Claire.prototype.deregister = function (callback) {
  if (this.registered_) {
    this.clearKeepaliveTimeout_();
    this.registerd_ = false;
    request({
      uri: this.config.karlUrl,
      method: 'DELETE',
      qs: { codename: this.codename }
    }, function (error, response) {
      if (typeof callback === 'function') {
        if (error) {
          callback({ success: false, error: { message: 'HTTP connection attempt failed' } });
        } else if (response.statusCode < 200 || response.statusCode > 299) {
          callback({ success: false, error: { message: 'HTTP request error', http_response: response } });
        } else {
          callback({ success: true });
        }
      }
    });
  }
};

Claire.prototype.forceSendKeepalive = function () {
  if (this.registered_) {
    // Clear existing keepalive timeout and restart the loop.
    this.clearKeepaliveTimeout_();
    this.keepaliveLoop_();
  }
};

Claire.prototype.keepaliveLoop_ = function () {
  if (this.registered_) {
    this.fillKeepaliveStatus_();
    this.sendKeepalive_();
    this.keepaliveTimeoutId_ = setTimeout(this.keepaliveLoop_.bind(this), this.keepaliveInterval);
  }
};

Claire.prototype.clearKeepaliveTimeout_ = function () {
  if (this.keepaliveTimeoutId_) {
    clearTimeout(this.keepaliveTimeoutId_);
    this.keepaliveTimeoutId_ = null;
  }
};

Claire.prototype.fillKeepaliveStatus_ = function () {
  var now = Date.now();
  var dependencies = (this.config.dependencies !== undefined && Array.isArray(this.config.dependencies) ?
      this.config.dependencies : []);
  var lastKeepaliveSent = (this.lastKeepaliveAttemptResult_.timestamp !== null ?
      timeIntervalAsHumanReadableString(this.lastKeepaliveAttemptResult_.timestamp, now) + ' ago' : '');
  var lastKeepaliveStatus = (this.lastKeepaliveAttemptResult_.status !== null ?
      this.lastKeepaliveAttemptResult_.status : '');
  var lastSuccessfulKeepalive = (this.lastSuccessfulKeepaliveTimestamp_ !== undefined ?
      timeIntervalAsHumanReadableString(this.lastSuccessfulKeepaliveTimestamp_, now) + ' ago' : undefined);
  var lastSuccessfulKeepalivePing = (this.lastSuccessfulKeepalivePing_ !== undefined ?
      String(this.lastSuccessfulKeepalivePing_) + 'ms' : undefined);
  var lastSuccessfulKeepalivePingUs = (this.lastSuccessfulKeepalivePing_ !== undefined ?
      this.lastSuccessfulKeepalivePing_ * 1000 : undefined);
  this.claireStatus_ = {
    service: this.config.service,
    codename: this.codename,
    local_port: this.config.localPort,
    cloud_instance_name: this.config.cloudInstanceName,
    cloud_availability_group: this.config.cloudAvailabilityGroup,
    dependencies: dependencies,
    reporting_to: this.config.karlUrl,
    now: now * 1000,
    start_time_epoch_microseconds: this.startTimestamp * 1000,
    uptime: timeIntervalAsHumanReadableString(this.startTimestamp, now),
    last_keepalive_sent: lastKeepaliveSent,
    last_keepalive_status: lastKeepaliveStatus,
    last_successful_keepalive: lastSuccessfulKeepalive,
    last_successful_keepalive_ping: lastSuccessfulKeepalivePing,
    last_successful_keepalive_ping_us: lastSuccessfulKeepalivePingUs,
    build: this.config.buildInfo,
  };

  if (this.statusFiller_) {
    this.claireStatus_.runtime = this.statusFiller_();
  }
};

Claire.prototype.sendKeepalive_ = function () {
  var beforeRequestTimestamp = Date.now();
  var that = this;

  request({
    uri: this.config.karlUrl,
    method: 'POST',
    qs: { codename: this.codename, port: this.config.localPort },
    json: true,
    body: this.claireStatus_
  }, function (error, response) {
    var afterRequestTimestamp = Date.now();
    that.lastKeepaliveAttemptResult_.timestamp = afterRequestTimestamp;
    if (error) {
      that.lastKeepaliveAttemptResult_.status = 'HTTP connection attempt failed';
    } else if (response.statusCode < 200 || response.statusCode > 299) {
      that.lastKeepaliveAttemptResult_.status = 'HTTP response code ' + String(response.statusCode);
    } else {
      that.lastKeepaliveAttemptResult_.status = 'Success';
      that.lastSuccessfulKeepaliveTimestamp_ = afterRequestTimestamp;
      that.lastSuccessfulKeepalivePing_ = afterRequestTimestamp - beforeRequestTimestamp;
    }
  });
};

module.exports = Claire;
