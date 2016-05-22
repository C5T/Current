# Karl

*Your service should be up and running. Up and running, Karl!*

## Objective

Current services typically run as separate binaries, exposing certain functionality on certain HTTP port. TL;DR: Karl is a tool to browse the topology of Current services, in its present and past state.

Karl is the dedicated supervisor binary for a set of running Current services. Normally, Karl would run on a separate server.

The way to have a Current service communicate with Karl is to use Claire. TL;DR: The scope of an instance of Claire maps to the scope of service lifetime during which it comunicates with Karl declaring itself up and running.

The general ideas behind Karl and Claire are:
* Claire communicates with Karl on a nice-to-have basis.
  * Unless configured othwerise, even if Karl is unreachable, Claire-powered client binary remains fully functional.
* Karl maintains historical state of the fleet, but can be run in stateless mode.
  * If a production Karl is replaced by a virgin-state one, it would be populated with the present state of the fleet within the next half a minute.
* Karl maintains the historical state of all keepalives it has received.
  * Basic Karl reporting page is a state of the fleet over certain period of time, "last five minutes" by default.
    * The report will be based on all keepalives received within this time period.
  * Drilldown pages include slicing per:
    * IP address (server/machine view),
    * IP address and port ("endpoint" view, the "socket" other services may depend upon),
    * Service name (service availability history), and
    * Codename (service instance history, down to individual keepalive messages).
* Both Claire and Karl are embedded C++ libraries.
  * Claire extends the user code (TODO(dkoroleV): ... injecting itself between port+route ownership acquisition and individual routes handlers initialization).
  * Karl is designed to be linked into the user code providing active supervision (ex. stream data authority master flip).

## Terminology

* **Service** name, ex. `"ctfo_server"`.
  A human-readable function of the service. By convention, a valid C++ identifier. Linked into the binary.

* **Codename**, ex. "ABCDEF".
  A random, unique, identifier of a running service. Regenerated on each binary run, changes with restart.

* **Claire status**
  A common denominator status report, containing the binary info, generic runtime info, and, if the service has completed initialization and informed Claire about it, the runtime info for this service.

* **Claire beacon**
  A scoped object, during the lifetime of which Claire periodically sends its status upwards to Karl. The default beacon keepalive frequency is 20 seconds (in Claire; and the default unhealthy interval in Karl is 45 seconds, twice this plus five seconds).

* **Keepalive**
  A periodic message sent from beacon-enabled Claire to Karl. Contains the Claire status.
  Karl persists all keepalive messages, except for build info which is only stored if it is different from the previously reported one from the binary with this codename. Effectively, "which is only stored once", as build info does not change at runtime.

* **Binary info**
  Service name, build date/time, compiler/environment info, git branch and commit hash, and whether the build was performed from a vanilla branch, etc. Generated at build time by a custom `Makefile` (`current.build`, ref. `Current/scripts/MakefileWithCurrentBuild`).

* **Generic runtime info**
  The generic part of Claire binary status: codename, uptime and local time, the time of the last keepalive accepted by Karl if already in talking-to-Karl mode.

* **Claire status page**
  A page available via a `GET`/`POST` request to `localhost:${port}[possible_prefix]/.current`. Returns a JSON with Claire status.
  
  This JSON, by convention, is always parsable as a generic Claire status (no service-specific runtime data).
  
  This JSON can be parsed as a Claire status with a custom user status type, as long as the latter is linked into Karl's typelist. This is the way to tailor Karl's status reporting and supervision to specific service needs.

* **Claire status page options**
  Just opening the `/.current` page from the browser yields the JSON without the build info (with `null` instead of it).
  
  To report a JSON with build info, request `?all` or `?a`. The `?all` page is what Claire reports to Karl in keepalives.
  
  To report build info only, use `?build` or `?b`.

## TODO

Once the prototype is done, this doc should be made up to date.

The prototype in its present form resembles the idea, but the implementation is skewed towards a different usecase. -- @dkorolev

Done:

* Prototyped Karl (server).
* Prototyped Claire (client).
* Base response (`ClaireStatusBase`)
* More complex response (`ClaireStatus`)
* Claire-to-Karl-to-Claire loopback registration.
* Karl keeping the state of the fleet and returning it.

Remains to code:

* Claire sending pings to Karl.
* Reporting dependencies.
* Visualization as .DOT (for unit testing) and .SVG (with browsing).
* A standalone test where several binaries are started, and Karl can be killed and re-started. Claire-s should not die.

# Obsolete

Karl is Current's monitoring, alerting, and continuous integration service.


## Motivation

Monitoring and alerting is a data problem. Current's mission is to solve data problems easily and efficiently. And we need monitoring as we grow.

And our love for stong typing and metaprogramming powers of C++ scales well to fleet configuration.

On top of it, monitoring and continuous integration are too tightly coupled to be separated.

## Example

### Client: Claire

Claire is a binary monitored by Karl.

```cpp
// Registers a `/current` HTTP endpoint on the specified port. It responds with a JSON containing:
// * `uptime_ms`:         The uptime of the binary.
// * `state`:             "StartingUp", "Running", or "TearingDown" <=> HTTP 503, 200 and 404 codes.
// * `state_uptime_ms`:   The time the binary has been running in this state, in millis.
// * `local_time`:        To ensure NTP is up and running locally.
// * `binary_build_time`: To track how up to date the presently running version is.
CLAIRE(FLAGS_port);
```

Also, Claire enables continuous integration with no extra work.

```cpp
// On startup, checks whether another Claire-powered binary is already running on this port.
//
// * If the same binary of the same build time is already running, it will keep running,
//   and the binary that was starting on top of it will silently exit with success exit code.
//
// * If the version already running is older than the one that is just being started,
//   the currently starting binary will gracefully tear down the older one and start itself.
//
// * In any other case, or in case of failure, an alert is fired.
//
// Exposes `/current?sigterm` and `/current?stop` to enable the above.
//
// The below statement should be the first one using the corresponding HTTP port,
// preceding any endpoints registering.
CLAIRE_WITH_CONTINUOUS_INTEGRATION(FLAGS_port);
```

The above design makes continuous integration as straightforward as inserting:
* `(cd $DIR; ./$BINARY`), or even
* `(cd $DIR; git pull; make $BINARY; ./$BINARY)  # Yay for Linux-way!`

into `crontab`.

To monitor extra fields, simple, trivially copyable types can be declared as `std::atomic_*`-s and registered as `CLAIRE_VAR()`-s. 

```cpp
// Exposes two additional, trivially copyable variables to monitor.
std::atomic<uint64_t> model_train_time_ms;
std::atomic<double> model_cross_validation_accuracy;

CLAIRE_VAR(model_train_time_ms);
CLAIRE_VAR(model_cross_validation_accuracy);

CLAIRE_WITH_CONTINUOUS_INTEGRATION(FLAGS_port);
```

Complex structures should be made serializable, and wrapped into `WaitableAtomic<>`-s to ensure thread safety.

```cpp
// Expose a user-defined structure to monitor.
CURRENT_STRUCT(ComplexStats) {
  CURRENT_FIELD(foo, std::string);
};

WaitableAtomic<ComplexStats> complex_stats;

CLAIRE_VAR(complex_stats);
CLAIRE_WITH_CONTINUOUS_INTEGRATION(FLAGS_port);
```


### Server

To use Karl:

1. Define a **DRI**, for example `dmitry.korolev@gmail.com` as `dima`.

   A DRI is defined by a unique name identifying the channel to reach an individual and the means to reach them.

2. Define a **Service**, for example, `backend`.

   A service is defined by a unique name under which servers are clustered, and under which alerts will be generated and pages will be sent.
   
3. Define a **Server**, for example `http://api.current.ai` as `api`.

   Each server is defined by its top-level URL to monitor, with `/current` added by default.
  
   Servers also have names, and each name should be unique within the service they belong to.

   For alerting, paging and muting purposes, each server belongs to one and only one service.

```cpp
DRI(dima, EMail("dmitry.korolev@gmail.com"));
SERVICE(backend);
SERVER(api, backend, "http://api.current.ai");

// The above are macroses to automatically assign DRI, SERVICE and SERVER the corresponding
// names under which they will be exposed not just as C++ objects internally, but also
// as REST-ful identifiers and JSON fields externally. Macroses are just a convenience,
// all `dima`, `backend` and `api` can be declared as regular variables, as long as
// they are passed their corresponding externally-facing names as constructor parameters.
```

The above three lines of code define Karl monitoring with default settings. Which are:

* Scrape `/current` every minute.
* Confirm the endpoint is accessible.
* Confirm the latency of each scrape is under 500ms.
* Confirm the latency of the 2nd slowest request in the past hour is under 100ms.
* Confirm the median latency of the past hour is under 10ms.
* Confirm the resulting JSON can be parsed.
* Confirm the client's state is "Running".
* Confirm the client's uptime has increased by 55 .. 65 seconds since the last scrape.
* Confirm the client's local time is within 5 seconds from server's local time.
* Confirm the client's binary version did not change since last scrape.

*Regarding client vs. server time:* It's embarasing, but AWS instances don't use NTP by default and often have local time off by minute(s). Synchronized local time is important for being data-driven, and we monitor it by default.

*Regarding changing binary version:* Karl purposely generates a dedicated alert every time a new binary is pushed or picked up by continuous integration, in addition to the naturally firing one regarding the uptime going down. Normally, these alerts can and should be temporarily muted by the engineer doing the regular push, see below.

To tweak the parameters of basic alerts:

```cpp
// This line is equivalent to the one from the above example.
SERVER(api, backend, "http://api.current.ai", BasicAlerts());

// For another endpoint, change polling interval from one minute to ten minutes.
// Other thresholds are adjusted accordingly.
SERVER(predictive_analytics,
       ctfo,
       "http://api.current.ai/ml",
       BasicAlerts().ScrapeInterval(10 * 60 * 1000));
```

Custom alerts are defined as:

```cpp
struct ModelAccuracyAlert {
  void OnScrape(const Scrape& scrape) {
    // Analyze the `scrape`.
    // Any exception here will generate an alert.
    // There is a predefined exception type for a well-formed alert.
    // NOTE: The alert does not necessarily correspond 1:1 to the scrape.
    // TODO(dkorolev): Sync up with Max about it. I have a master plan.
  }
};

SERVER(predictive_analytics,
       ctfo,
       "http://api.current.ai/ml",
       BasicAlerts(),
       CustomAlert<ModelAccuracyAlert>());
```

Also, each alert has severity. (`TODO(dkorolev): Max, let's fix the list. Ex. CRITICAL > ERROR > WARNING > FYI. I love generic solutions, but here it seems unnecessary.`)

By default, every non-muted alert is sent to every DRI and automatically muted for the next 15 minutes. The configuration of alert dispatching is documented below.

## Design

Below is the data dictionary for Karl. All entities are browsable via a REST-ful API.

#### DRI

**A Directly Responsible Individual who may receive alerts.**

Has a unique name. For the purposes of this doc, assume they have an e-mail address on file.
  
  
#### Service

**A collection of servers to monitor.**

Has a unique name. Each Server belongs to one and only Service.

#### Server: A URL to connect to a binary that should be up and running

**Effectively, { IP address / DNS name, port, HTTP route }.**

Belongs to one and only Service. Has a unique name within the Service.
  
 
#### Scrape: A timestamped event of gathering status of a particular server

**Identified by { Timestamp, Service::Server }.**

ID space matches the one of Results.

#### Result: The result of a scrape.

**Identified by { Timestamp, Service::Server }.**

ID space matches the one of Scraped.

#### Alert: An event that is abnormal and should be reported

**Identified by { Service, Name, Timestamps }.**

**Optional { Server, Tags }.**

Name and tags are used for muting, see below.

Most alerts point to specific Scrape-s + Result-s for easier browsing.

#### Page: The alert that has been sent to the DRI

**Identified by { Alert, DRI }.**

Generally, muted alerts are not sent out as pages (yet can be browsed), and everything else makes it to one or more DRI.

#### Mute: A temporary hold placed on an Alert for it to not generate Pages

**Identified by { Service, Alert Name }.**

**Optional: { Alert Tags }.**

#### Dispatcher: The logic that generates pages given alerts and mutes

A single instance per Karl.

By default, muted alerts do not become pages, and non-muted ones are promoted to pages and sent to all DRIs. Paged alerts are then automatically muted by their { Service, Name }, any tags, for the next 15 minutes.

Dispatcher contains the more sophisticated logic that takes into consideration:
* current muting configuration
* alert severity
* DRI configuration
* alert delivery media (e-mail, SMS, etc.)
* time of day
* vacations calendar
* etc.

and makes the decision as per which alerts to promote to pages, and whom to deliver them using which media.

## Implementation

Alerts are defined and configured in a C++ file. No bare strings are used; each entity is defined as a variable of certain type, and then passed over.

A running Karl exposes a dashboard. Karl is also Claire: it is safe to run under `cron`, and it is friendly with continuous integration.

Everything is stored in Current Storage, with a REST-ful browsing API for viewing current configuration, scrapes, their results, alerts, pages, and mutes. Mutes can be set, prolonged/shrinked, and unset via this API.

`TODO(dkorolev): Discuss aggregation master-plan with Max.`

For Claire binaries requiring extra time to start up or tear down, pass `karl::WarmupPeriod(FLAGS_warmup_period_ms)` and/or `karl::ShutdownPeriod(FLAGS_shutdown_period_ms)`. This will tell Claire to allow for more time before switching form graceful to non-graceful shutdown of an already running binary in case it is expected by design to take more than the default three seconds to start up or to tear down.

The state can be changed programmatically from "StartingUp" to "Running" and from "Running" to "ShuttingDown". Use `CLAIRE_DECLARE_BINARY_RUNNING()` and `CLAIRE_DECLARE_BINARY_TERMINATING()` to change the state programmatically. Externally exposed state will also be used for load balancing.
