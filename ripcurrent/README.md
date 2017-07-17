# Flow

`Flow` is the language to define data pipelines. Based on an idea originally called `TailProduce` ([design doc](http://dimakorolev.quora.com/TailProduce-Design-Doc))

## Design

1. **Define continuous pipelines**, directly in code.
2. **Strongly typed**, compiled, zero overhead, designed to get the most out of one machine.
3. **Respects statefulness** of each individual worker, while staying stateless as the whole.

## Teaser

A production service, serving external API, internal endpoints, generating snapshots of labeled data for model training, and exposing API for applying predictive analytics at real time.

```cpp
// The stream of raw event log to operate on.
auto raw_log_stream = Stream<RawLogStreamSchema>(
  "raw_log",
   Persistence::PersistToDisk,
   Access::PubSubEnabled("/api/raw_log");

// Data pipeline: API + predictive analytics + reporting.
(ListenToRawLog(raw_log_stream) |
 // Serve the top-level API endpoints, that require nothing but the raw log.
 Thru<ListenToBasicAPIEndpoints>() | Thru<ServeBasicAPICall>() |
 // Split the raw log by sessions, then by users.
 GroupEventsBySessions() |
 GroupSessionsByUsers() |
 // Label users with a binary feature "came back within the past one week".
 (PassThru() +
  (ExtractOnlyUserIDs() | 
    (Annotate(Retention::Now) + (DelayBy(DAY * 7) | Annotate(Retention::Previous))) |
    LabelUsersByRetention("1w"))
 ) |
 // Serve the user-level API endpoints, with access to the annotations generated above.
 Thru<ListenToUserAPIEndpoints>() | Thru<ServeUserAPICalls>() |
 // Maintain sliding windows of one hour, one day, and one week.
 (Annotate("1h+") + (DelayBy(HOUR)  | Annotate("1h-")) + 
  Annotate("1d+") + (DelayBy(DAY)   | Annotate("1d-")) + 
  Annotate("1w+") + (DelayBy(DAY*7) | Annotate("1w-"))) |
 // Serve the sliding-window based API endpoints, exposing training data snapshots.
 Thru<ListenToMLFeaturesEndpoints>() | Thru<ServeMLFeaturesAPICalls>() |
 Thru<ExposeFeaturesForTrainingData>() | Thru<ExposeMLSnapshotEvery>(HOUR) |
 Thru<ServeMLFeaturesSnapshot>() |
 Thru<ApplyPredictiveAnalyticsModels>()
).Flow();
```

## Examples

### Continuous Run

Continuously count distinct English words by maintaining a histogram of their usage frequency in an input stream of words.

```cpp
// Forever parse the standard input word by word,
// and maintain the histogram of the words seen so far.
(ParseByWords(cin) |
 ConvertWordToLowerCase() |
 OnlyPassLatinWords() |
 MaintainHistogram()).Flow();
```

The above code will run forever, delivering each word comprised of English characters to an instance of a class that has declared the histogram as a stateful data member, and keeps maintaing its consistency.

This code is textbook example, the histogram itself is not yet exposed anywhere.

### Message Typing

The canonical way to expose the histogram is to introduct a custom message type.

```cpp
(ParseByWords(cin) |
 ConvertWordToLowerCase() |
 PassLatinWordsAndCustomMessages() |
 MaintainAndOutputHistogram()).Flow();
```

C++ overloading mechanisms are used to differentiate messages by types. Conditional statements on event types, as well as on their field presence and types, is retired in favor of a structured data dictionary.

```cpp
// An empty message to asynchronously pass the signal.
MESSAGE(DumpHistogramMessage) {};

// The logic to pass in the incoming word, the above message,
// or the termination signal.
SURFER(PassLatinWordsAndCustomMessages) {
  bool f(const string& word) {
    if (IsLatinWord(word)) {
      emit(word);
    } else if (word == "!") {
      emit(DumpHistogramMessage());
    } else {
      return (word != ";");
    }
  }
};

// The logic to keep the histogram consistent and dump it
// to standard output as requested.
SURFER(MaintainAndOutputHistogram) {
  map<string, int> histogram;
  void f(const string& word) {
    ++histogram[word];
  }
  void f(DumpHistogramMessage) {
    for (const auto& e : histogram) {
      cout << e.first << " : " << e.second << endl;
    }
  }
};
```

The framework ensures that, while `PassLatinWordsAndCustomMessages` and `MaintainAndOutputHistogram` will, by default, be run in different threads, message passing is sequential and synchronous, and thus no locking is necessary.

### Joined Inputs

Various sources of messages can be joined, with the framework taking care of concurrency.

```cpp
// The message requesting to dump the histogram into an accepted HTTP connection.
// Carries the HTTP request object across threads to where it will be fulfilled.
// Uses Current HTTP API syntax in the definition and below.
MESSAGE(DumpHistogramHTTPRequest) {
  Request request;
};

// The origin of the input stream of the relevant HTTP requests.
ORIGIN(ServeIncomingHTTPRequests) {
  ServeIncomingHTTPRequests() {
    HTTP(FLAGS_port).Register("/dump", [this](Request request) {
      emit(DumpHistogramHTTPRequest{move(request)});
    });
  }
};

// An example pipeline that joins the inputs of incoming words and HTTP requests.
(ParseByWords(cin) |
 ConvertWordToLowerCase() |
 OnlyPassLatinWords() + ServeIncomingHTTPRequests() |
 WordsHistogramWithRESTfulAPI()).Flow();

// The histogram data dictionary, augmented by the logic
// of maintaining its consistency and exposing it via a RESTful endpoint.
SURFER(WordsHistogramWithRESTfulAPI) {
  map<string, int> histogram;
  void f(const string& word) {
    ++histogram[word];
  }
  void f(DumpHistogramHTTPRequest http_request) {
    http_request.request(Response(
      Join(Map(histogram, [](const std::pair<string, int>& word) {
          return word.first + ':' + word.second;
        }, '\n'),
      HTTPResponseCode.OK));
  }
};

```

### Stateful Workers

A more real-life example of continuously keeping track of click-through-rate of unique users over unique ads.

```cpp
SURFER(CTRTracker) {
  // Data dictionary: collections of unique impressions and clicks.
  struct PerUser {
    set<CONTENT_ID> impressions;
    set<CONTENT_ID> clicks;
    double CTR() const {
      if (impressions.empty()) {
        return 0.0;
      } else {
        return 1.0 * clicks.size() / impressions.size();
      }
    }
  };
  map<USER_ID, PerUser> users;
  
  // Of the incoming events from the product, only consider `Impression` and `Click`.
  // Other events will be ignored.
  void f(Impression i) {
    users[i.user_id].impressions.insert(i.content_seen_id));
  }

  void f(Click c) {
    auto& user = users[c.user_id];
    auto content_id = c.content_clicked_id;
    user.impressions.insert(content_id);
    user.clicks.insert(content_id);
  }
  
  // Also, handle external HTTP API requests.
  f(CTRPerUserRequest request) {
    const auto cit = users.find(request.user_id);
    if (cit != users.end()) {
      request.RespondViaHTTP(Response(cit.CTR(), HTTPResponseCode.OK));
    } else {
      request.RespondViaHTTP(Response("User not found.", HTTPResponseCode.NotFound));
    }
  }
};
```

By this point, adding a RESTful call to report ten top performing ads, perhaps even filtered by the ones a particular user has not seen yet, becomes a simple exercise.

### Origin and Pond

The node that accepts no messages but can emit some is called an Origin. `ServeIncomingHTTPRequests` is an example of one from the above.

The node that emits no messages but can accept some is calles a Pond. An example of Pond is `SaveTo()`. While not used explicitly, a pipeline:

```cpp
(ParseByWords(cin) |
 ConvertWordToLowerCase() |
 PassLatinWordsAndCustomMessages() |
 MaintainAndOutputHistogram()).Flow();
```

is conceptually equivalent to:

```cpp
auto tmpfile = "intermediate.tmpfile";

auto stage1 = (ParseByWords(cin) |
               ConvertWordToLowerCase() |
               SaveTo(tmpfile));
auto stage2 = (LoadFrom(tmpfile),
               PassLatinWordsAndCustomMessages() |
               MaintainAndOutputHistogram());

auto flow1 = stage1.FlowAsync();
auto flow2 = stage2.FlowAsync();

flow1.Join();
flow2.Join();
```

where `SaveTo()` declares a pond into which the first current flows, which is used as a `LoadFrom()` origin for the second current to flow from.
