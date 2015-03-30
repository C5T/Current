# Sherlock Design Doc

## Terminology

A **stream** is a persistent, sequential, structured, immutable, append-only storage of data.

The quantum storage unit of the stream is an **entry**.

Entries can be appended to streams using **publishers** and scanned with **listeners**.


## Entries

Streams are strongly typed, and within a stream, each entry has the type specified at stream declaration time. All entries within one stream have the same type. The type can, and often is, polymorphic.

Implementation-wise, entries of polymorphic type are `std::unique_ptr<>`-s of `SomeBaseType`: the user they call their virtual functions or have individual instances of `[const] SomeBaseType&` dispatched via other RTTI mechanisms.

The type of entries in the stream is defined at stream declaration time. When serializing/deserealizing streams to/from disk, a basic check of the signature of storage schema is performed to verify that no data corruption may occur.

Polymorphic types can be extended with more derived types, but it is the user's responsibilty that no binary attempts to deserialize entries of the type not supported by this particular version. (`TODO(dkorolev): See how strict we can be with this in V1.`)

In order to qualify for a stream type, it should be guaranteed that an entry of this type can be serialized and de-serialized, and that the very same entry will be constructed as a result of the serialization/de-serialization cycle. In other words, stream entries should be agnostic with respect to serialization. This is a requirement for dropping strict guarantees on the number of serializations that take place internally.

Implemenation-wise, Sherlock uses Cereal, and we use JSON and binary formats.

## Consistency

Each entry of a stream is assigned an **index**: an `uint64_t` value equal to the number of entries preceeding the one being considered, effectively, a 0-based index.

Additionally, a stream defines the **order key type**. One can think of the order key as the timestamp.

Order keys often are indeed timestamps, represented in certain way, for example, Unix epoch time in milliseconds.

It is the user's responsibility to ensure that order keys of consecutive entries follow a strictly increasing order. (`TODO(dkorolev): I am still debating whether "non-decreasing order" is worth it here, leaning towards "no".`).

This guarantee enables `TailProduce`: A paradigm for real-time data crunching algorithms based on `producers` accepting data joined at runtime from multiple streams on their respectiveve order keys.

Implementation-wise, the mechanism to extract order keys from entries is a member function that can be templated or overloaded. Thus, there are no constraints on how many various values can be used as order keys per entry type. Keep in mind though, that if the entry type is polymorphic, in order for certain order key type to be supported it is required for a getter of the order key of this type to be a (virtual) function of the top-level base class of this polymorphic type.

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

As for the starting enrty for the listener, there are ways to start the listener from a specific point in the past, specifically, from an entry with certain index, or from an entry with certain order key. The former is used for stream replication, the latter is used for TailProduce jobs, where the producer guarantees that its state at certain "time == order key" is agnostic with respect to how many entries preceeding this order key minus a fixed, specified time window width.

As for the end of the listener, it is possible to request the listener to terminate automatically after certain number of entries have been scanned or after certain order key has been reached. On top of that, the listener itself can choose to stop itself when certain criteria has been met. Since listeners are independent, the termination criteria does not have to be consistent, even within the very same listener.

Implementation-wise, listening to a stream spawns a dedicated thread. (`TODO(dkorolev): The design enforces the users to be "good citizens", and not spawn listeners in large quantities, however, in certain cases one thread per listener might not be the most efficient approach. I should document it in a bit more detail.`).

Being run in dedicated threads, listeners never interfere with each other and with the publisher. It is legit for a very slow listener to run in parallel with the one that is expected to always be caught up with the most recently added entires.

A per-entry handler of a listener has the means to tell how far "behind" on a stream it is, both index-wise and order-key-wise. This is important for serving guarantees: for example, when a listener is responsible for serving external requests, the common design is to notify its master or load balancer that it is healthy as it is caught up and that it still is in the "replay log" mode.

### Publishing

While publishing, entries should come in increasing order of order keys.

By design, a `Publish()` operation is not thread-safe, and it is advised to acquire stream's publisher within a specific thread that does the publishing.

Implementation-wise, within KnowSheet, this thread would almost always be an HTTP server, a consumer of an in-memory message queue, a listener of another stream or a producer of a TailProduce job. All of the above are single-threaded by design, and they respect the order of calls.

Streams are hardly deleted: in most production systems, if a stream is created it persists for the lifetime of the binary.

With that in mind, however, it is possible to shutdown a stream without terminating the binary. The shutdown operation is designed to be as safe as possible: implementation-wise, it will wait for all pending calls to currently active listeners, as well as signal the acquired publisher to terminate. Only after all the handles have been released, the stream will be destructed.


## Replication

From data integrity standpoint, each stream is assigned an **authority**: A binary, running, usually a dedicated machine, that is responsible to reporting the current up-to-date status of the stream.

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

The reader of this design doc is expected to be familiar with `KnowSheet/Brick`, most notably, the Cereal part of it ("cerealize"), the HTTP API ("net/api"), the in-memory message queue ("mq/inmemory") and WaitableAtomic ("waitable_atomic").

## Naming

Streams get names assigned at initialization. Stream names must be unique.

## Protocol

HTTP chunked response. Cereal's binary or JSON format, controlled by the URL parameter.

HTTP endpoints are exposed automatically, based on stream names.

