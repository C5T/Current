# Sherlock Design Doc

**Sherlock** is a persistent strongly typed publish-subscribe-friendly message queue.

## Terminology

**Stream** is a persistent, sequential, structured, immutable, append-only storage of data.

The quantum storage unit of the stream is an **entry**. Each entry in the stream must have an **index** and may have one or more **order keys** of various types.

Entries can be appended to streams using **publishers** and scanned with **listeners**.


## Entries

Streams are strongly typed. All entries within one stream have the same type. This type can be, and often is, polymorphic.

The type of entries in the stream is defined at stream declaration time. When serializing/deserializing streams to/from disk, a basic check of the signature of storage schema is performed for the sake of data integrity.

Polymorphic types can be extended with more derived types, but it is the user's responsibilty that no binary attempts to deserialize entries of the type not supported by this particular version. (`TODO(dkorolev): See how strict we can be with this in V1.`)

Entry stream type should be agnostic with respect to serialization. It should be guaranteed that an entry of this type can be serialized and deserialized, and that the very same entry will be constructed as a result of the serialization/deserialization cycle.

Implemenation details:

* Sherlock uses Cereal, JSON and binary formats.
* Entries of polymorphic type are `std::unique_ptr<>`-s of `SomeBaseType`.
* RTTI dispatching mechanisms from `Bricks` can be used for convenience of saving on virtual functions.

## Consistency

Each entry of a stream is assigned an **index**: an `uint64_t` value equal to the number of entries preceeding the one being considered. Effectively, the entry's 0-based index in the stream.

Additionally, a stream defines the **order key type**. One can think of the order key as the timestamp.

Order keys often are indeed timestamps, represented in certain way, for example, Unix epoch time in milliseconds.

It is the user's responsibility to ensure that order keys of consecutive entries follow the non-decreasing order.

This guarantee enables `TailProduce`: a paradigm for real-time data crunching algorithms based on `producers` accepting entries from multiple streams joined at runtime, maintaining the order of their respective order keys.

Order key type is defined at the stream level. Individual entry types can support multiple order key types.

Implementation details:

* By default, the order key duplicates `index` and has the type `uint64_t`.
* To support order keys other than `index`, the mechanism to extract the order key of certain type should be provided.
* The easiest way to provide this mechanism is to define a `void ExtractOrderKey(T& output_order_key);` method for the type defining the entry.
* For polymorphic types this method should be virtual.

`TODO(dkorolev): Yes, I will rename the method(s) in the code soon.`

## Persistence

Streams are stored persistently on disk.

The default design stores binary-serialized entires in append-only files. The framework keeps one *active*, currently appended to, file, and a directory of already *finalized* files.

Default file finalization policy uses file size cap, max. number of entries per one file and max. age of a file. Default settings are 100M entries, as 10MB files, containing no more than 24 hours time window of a file.

For faster lookup, finalized files include first and last indexes, as well as first and last order keys, as part of the filename. Thus, for finalized files, only their names have to be scanned in order to be able to efficiently create listeners.


## Dataflow

Fundamentally, a stream supports two operations:

1. **Listen**: Run certain handler for each entry from the stream, potentially indefinitely.
2. **Publish**: Add a new entry to the stream.

### Listeners

Listener is a callback that is invoked per each stream entry, in the order the entries have been added.

By default, the listener starts at the beginning of a stream and never stops.

As for the starting entry for the listener, there are ways to start the listener from a specific point in the past, specifically, from an entry with certain index, or from an entry with certain order key. The former is used for stream replication, the latter is used for TailProduce jobs, where the producer guarantees that its state at certain "time == order key" is agnostic with respect to how many entries preceeding this order key minus a fixed, specified time window width.

As for the end of the listener, it is possible to request the listener to terminate automatically after certain number of entries have been scanned or after certain order key has been reached. On top of that, the listener itself can choose to stop itself when certain criteria has been met. Since listeners are independent, the termination criteria does not have to be consistent, even within the very same listener.

#### Operation

A dedicated thread is spawn per listener. Being run in separate threads, listeners never interfere with each other and with the publisher. It is legit for a very slow listener to run in parallel with the one that is expected to always be caught up with the most recently added entires.

Generally, listeners start by processing entries from the past. In this mode, the listener never waits, as the next entry is ready to be processed as soon as the processing of the previous one is done.

Once processing of the entries from the past is done, the thread that runs the listener waits until new entries appear. New entries are then processed immediately as they are published.

The former is called **log replay**, and the listener is considered to be **behind** in this mode. The latter listener is said to be **up to date** or **caught up**.

The natural order of things for the listener is to start in "behind" mode and eventually get "caught up". Note that, however, it is not impossible for the listener to change its state from "caught up" back to "behind", if it turns out that its processing rate became slower that the rate at which new entries are being added to the stream.

#### Batch Mode

Listeners for batch-mode processing jobs are agnostic as to whether they are behind or caught up.

#### Real-Time Mode

In more sophisticated cases, the code powered by new data coming via a listener may wish to have its in-memory state exposed in a asynchronous way.

Recommended solutions in the order of preference are;

1. Push incoming requests into another stream and have the listener operate on the join of these two streams.
2. Push both stream inputs and incoming requests into a message queue, where they will be interleaved.
3. Old-school mutexes, which might be beneficial in cases where many input entries do not alter the state and read-write locking is faster.

### Publishing

While publishing, entries should come in increasing order of order keys.

##### Acquisition

By design, a `Publish()` operation is not thread-safe. To remove room for error, stream's **publisher** has to be acquired. Each stream has only one publisher to acquire; an attempt to acquire the publisher twice results in a run-time error.

##### Stream Deletion

Streams are hardly deleted. In most production systems, if a stream is created it persists for the lifetime of the binary.

With that in mind, however, it is possible to shutdown a stream without terminating the binary. The shutdown operation is designed to be as safe as possible: it waits for all pending calls to currently active listeners, as well as signal the acquired publisher to terminate. Only after all the handles have been released, the stream will be destructed.

Implementation details:
* Within KnowSheet (outside TailProduce), the listening thread would almost always be an HTTP server, a consumer of an in-memory message queue, a listener of another stream or a producer of a TailProduce job. All of the above are single-threaded by design, and they respect the order of calls.
* The waiting part of stream destruction happens in the destructor of the instance of the stream.


## Replication

From data integrity standpoint, each stream is assigned an **authority**: a binary, running, usually on a dedicated machine, that is responsible for reporting the current up-to-date status of the stream.

Sherlock uses master-slave replication. Replication logic is to indefinitely listen to the entries from the index that is the first one not yet published into the local copy of the stream.

(`TODO(dkorolev): Say a few words about our failover philosophy.`)

Data from streams can be replicated. Replication can be thought of as a simple usecase of:

1. A listener factory bound to an HTTP endpoint opening a chunked response HTTP connection ("everybody can 'clone' the stream knowing its name"), and
2. A publisher, ownership of which is transferred into the HTTP chunked response receiver, so that the only legitimate was to add entries to the stream

(`TODO(dkorolev): Security and authentication.`)

The recommended usecase of Sherlock is to replicate each data stream locally before working with it. This ensures that between re-runs of the binary, only the newly added entries are read by the master transferred over the netwoe

## HEAD Pointer

TBD: A way to notify that the order key has updated without new entries being added, required for real-time stream joining in `TailProduce`.


# Implementation Notes

`TODO(dkorolev): Code snippets?`

`TODO(dkorolev): Move all implementation details from above into this section?`

## Background

The reader of this design doc is expected to be familiar with `KnowSheet/Bricks`, most notably, the Cereal part of it ("cerealize"), the HTTP API ("net/api"), the in-memory message queue ("mq/inmemory") and WaitableAtomic ("waitable_atomic").

## Naming

Streams get names assigned at initialization. Stream names must be unique.

## Protocol

HTTP chunked response. Cereal's binary or JSON format, controlled by the URL parameter.

HTTP endpoints are exposed automatically, based on stream names.

