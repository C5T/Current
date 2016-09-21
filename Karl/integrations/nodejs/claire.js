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

function timeIntervalAsHumanReadableString(begin_date, end_date) {
  var begin_ms = begin_date.getTime();
  var end_ms = end_date.getTime();

  if (end_ms < begin_ms) {
    return '-' + timeIntervalAsHumanReadableString(end_date, begin_date);
  }

  var seconds = Math.floor((end_ms - begin_ms) / 1000);
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
  const startDate = new Date();
  const codename = generateCodename();
  this.lastKeepaliveAttemptResult = { date: null, status: null };
  this.keepaliveLoopRunning = false;

  Object.defineProperty(this, 'config', { value: config, writable: false });
  Object.defineProperty(this, 'codename', { value: codename, writable: false });
  Object.defineProperty(this, 'startDate', { value: startDate, writable: false });
  Object.defineProperty(this,
                        'keepaliveInterval',
                        {
                          value: (config.keepaliveInterval !== undefined) ? config.keepaliveInterval : 20000,
                          writable: false
                        });
}

Claire.prototype.constructor = Claire;

Claire.prototype.register = function (statusFiller) {
  Object.defineProperty(this, 'statusFiller', { value: statusFiller, writable: false });
  this.keepaliveLoopRunning = true;
  this.keepaliveLoop();
}

Claire.prototype.deregister = function () {
  this.keepaliveLoopRunning = false;
  request({
    uri: this.config.karlURL,
    method: 'DELETE',
    qs: { codename: this.codename }
  }, function () {});
}

Claire.prototype.forceSendKeepalive = function () {
  this.fillKeepaliveStatus();
  this.sendKeepalive();
}

Claire.prototype.keepaliveLoop = function () {
  if (this.keepaliveLoopRunning) {
    this.fillKeepaliveStatus();
    this.sendKeepalive();
    setTimeout(this.keepaliveLoop.bind(this), this.keepaliveInterval);
  }
}

Claire.prototype.fillKeepaliveStatus = function () {
  var now = new Date();
  var dependencies = (this.config.dependencies !== undefined && this.config.dependencies.isArray) ?
      this.config.dependencies : [];
  var lastKeepaliveSent = (this.lastKeepaliveAttemptResult.date !== null) ?
      timeIntervalAsHumanReadableString(this.lastKeepaliveAttemptResult.date, now) + ' ago' : '';
  var lastKeepaliveStatus = (this.lastKeepaliveAttemptResult.status !== null) ?
      this.lastKeepaliveAttemptResult.status : '';
  var lastSuccessfulKeepalive = (this.lastSuccessfulKeepaliveDate !== undefined) ?
      timeIntervalAsHumanReadableString(this.lastSuccessfulKeepaliveDate, now) + ' ago' : undefined;
  var lastSuccessfulKeepalivePing = (this.lastSuccessfulKeepalivePing !== undefined) ?
      String(this.lastSuccessfulKeepalivePing) + 'ms' : undefined;
  var lastSuccessfulKeepalivePingUs = (this.lastSuccessfulKeepalivePing !== undefined) ?
      this.lastSuccessfulKeepalivePing * 1000 : undefined;
  this.claireStatus = {
    service: this.config.service,
    codename: this.codename,
    local_port: this.config.localPort,
    cloud_instance_name: this.config.cloudInstanceName,
    cloud_availability_group: this.config.cloudAvailabilityGroup,
    dependencies: dependencies,
    reporting_to: this.config.karlURL,
    now: now.getTime() * 1000,
    start_time_epoch_microseconds: this.startDate.getTime() * 1000,
    uptime: timeIntervalAsHumanReadableString(this.startDate, now),
    last_keepalive_sent: lastKeepaliveSent,
    last_keepalive_status: lastKeepaliveStatus,
    last_successful_keepalive: lastSuccessfulKeepalive,
    last_successful_keepalive_ping: lastSuccessfulKeepalivePing,
    last_successful_keepalive_ping_us: lastSuccessfulKeepalivePingUs,
    build: this.config.buildInfo,
  };

  if (this.statusFiller !== undefined) {
    this.claireStatus.runtime = this.statusFiller();
  }
}

Claire.prototype.sendKeepalive = function () {
  var thisClaire = this;
  var beforeRequestDate = new Date();

  request({
    uri: this.config.karlURL,
    method: 'POST',
    qs: { codename: this.codename, port: this.config.localPort },
    json: true,
    body: this.claireStatus
  }, function (error, response) {
    var afterRequestDate = new Date();
    thisClaire.lastKeepaliveAttemptResult.date = afterRequestDate;
    if (error) {
      thisClaire.lastKeepaliveAttemptResult.status = 'HTTP connection attempt failed';
    } else if (response.statusCode < 200 || response.statusCode > 299) {
      thisClaire.lastKeepaliveAttemptResult.status = 'HTTP response code ' + String(response.statusCode);
    } else {
      thisClaire.lastKeepaliveAttemptResult.status = 'Success';
      thisClaire.lastSuccessfulKeepaliveDate = afterRequestDate;
      thisClaire.lastSuccessfulKeepalivePing = afterRequestDate.getTime() - beforeRequestDate.getTime();
    }
  });
}

module.exports = Claire;
