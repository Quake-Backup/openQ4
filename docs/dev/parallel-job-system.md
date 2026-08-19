# Portable Job System

openQ4 now owns a portable bounded job service for coarse engine work. It is
available in client, tool, and dedicated builds without changing Quake 4 asset,
save, demo, or network formats. The service is a foundation for later loading,
cache, animation, and renderer-front-end consumers; no stock asset work has
been moved off-thread yet.

The architecture was informed by id Software's GPLv3
[Doom 3 BFG `idParallelJobList`](https://github.com/id-Software/DOOM-3-BFG/blob/1caba1979589971b5ed44e315d9ead30b278d8b4/neo/idlib/ParallelJobList.h).
The implementation is original openQ4 code. It deliberately replaces BFG's
platform-specific worker assumptions and spinning list wait with portable C++
threads and condition variables, explicit saturation, and cooperative
cancellation. Both projects use GPLv3-compatible licensing; no BFG source file
was copied into this subsystem, so the audited BFG-lineage source-file count is
unchanged.

## Runtime policy and lifecycle

`idCommon` initializes the engine-owned service before commands and higher-level
runtime systems can submit work. Shutdown stops admission, requests cancellation,
waits for running callers and workers, and joins every worker before network,
session, renderer, or other subsystem teardown begins.

The startup-only controls are:

- `jobs_enable 1` enables threaded scheduling. Setting it to `0` never discards
  work; accepted lists execute inline in deterministic insertion order.
- `jobs_deterministic 1` selects the same synchronous implementation for tests
  and debugging. Dedicated builds default this setting to `1`, so they create no
  background job workers unless explicitly configured otherwise; threaded
  client execution requires `jobs_enable 1` with `jobs_deterministic 0`.
- `jobs_numThreads 0` reserves one hardware thread when possible and clamps the
  worker count to 1-32. An explicit value selects that bounded count.
- `jobs_queueCapacity 64` bounds admitted lists and accepts values from 1-1024.

Synchronous work participates in the same active-list and quiescence contract as
threaded work. A concurrent `CancelAll`, `WaitAll`, or shutdown therefore sees
and joins an inline list running on another submitting thread. Concurrent inline
submissions sleep until their submit sequence reaches the front rather than
spinning.

## Submission contract

Each `idJobList` declares fixed job and dependency capacities when it is created.
The implementation also fails closed above 1,048,576 jobs or 256 dependencies
per list. Combined with the configured active-list bound, this places a finite
upper limit on admitted work. Scheduler list and worker storage is reserved
before workers start; admission, dependency refresh, cancellation, and teardown
do not grow scheduler-owned containers. Job functions receive their payload,
worker index, and an `idJobCancellationToken`; payload ownership and lifetime
remain the caller's responsibility through the list's terminal state.

Admission returns an explicit `idJobSubmitResult`. In particular,
`QUEUE_FULL` leaves the list in its building state so the owner can wait for its
own safe join point and retry. Work is never silently dropped or placed in an
unbounded overflow queue. Callers must handle every non-success result; the
service does not guess whether blocking, inline fallback, or cancellation is
safe for a consumer.

Dependencies are deliberately chronological: a list may depend only on a list
already submitted to the same service. Self-dependencies, duplicate edges,
foreign-service lists, unsubmitted dependencies, and reverse edges into an
already-submitted list fail closed. This makes a dependency cycle
unrepresentable. A dependent list sleeps until all prerequisites complete and
is cancelled if a prerequisite fails or is cancelled.

Lists have low, normal, or high priority. Higher priority wins initially, while
every skipped runnable list gains a scheduling age. At the starvation threshold,
aged lists enter a primary oldest-age selection class which ignores base
priority; equal ages use the monotonic submit sequence. A continuously runnable
low-priority list therefore runs after a bounded number of dispatches even when
nine or more earlier high-priority lists remain runnable. Selection is
deterministic even though completion timing with multiple workers is necessarily
concurrent.

Cancellation is cooperative. Jobs that have not started are accounted for and
never invoked; running jobs must poll `context.IsCancellationRequested()` and
leave their own data in a safe state. List waits, worker waits, dependency waits,
and shutdown waits all use condition variables. There is no spin or sleep-poll
loop in the scheduler.

## Diagnostics and validation

`jobsStats` prints current admission, queue high-water, running/sleeping worker,
completion, rejection, cancellation, failure, wake, priority-promotion,
execution-time, and wait-time counters. Per-list status and timing counters are
also available through the C++ API.

`jobsSelfTest` exercises engine-service execution, dependency ordering, batch
work, and counters in either threaded or synchronous mode. Automation should
require the exact versioned success prefix:

```text
jobsSelfTest PASS v1
```

Engine shutdown emits this exact clean-quiescence marker only after all accepted
work has reached a terminal state and every worker has joined:

```text
jobsShutdown PASS v1 initialized=0 queued=0 running=0
```

The focused native test covers deterministic inline execution, worker
parallelism, fixed capacities, retryable saturation, dependency and failure
propagation, self/cycle rejection, cooperative cancellation, low-priority aging,
threaded shutdown, and cancellation/join of an inline job submitted from another
thread. `tools/tests/parallel_job_system.py` keeps the production lifecycle,
public contract, build registration, and documentation wired into the default
validation suite.

## Consumer boundary and promotion evidence

The first consumers should have immutable inputs, caller-owned result storage,
and an explicit main-thread join point: learned preload discovery, decode or
transcode stages, and generated-cache preparation are suitable candidates.
Archive mutation, live game-state mutation, and renderer API calls from arbitrary
workers are not.

The current light-grid CPU integration lives inside a dynamically loaded renderer
module. That module does not import the engine's `jobSystem` through
`RenderModuleAPI`, so wiring it directly would create an unresolved/duplicate
service boundary and unclear shutdown ownership. Light-grid work should adopt
the service only after a narrow renderer job import or engine-owned work packet
contract is designed; it is not a safe first consumer in this change.

Because no production consumer has migrated yet, `jobs_enable 1` and
`jobs_enable 0` execute the same stock workload outside the self-test. The
2026-08-19 current-build validation closed the local Milestone A lifecycle gate:
jobs-on and jobs-off `game/storage1` produced identical engine TGA bytes and
matching game state; both OpenGL modes and jobs-on Vulkan completed the repeated
`game/mcc_2` -> `game/storage1` -> `game/storage2` -> `game/storage1` ->
`game/tram1` campaign with clean shutdown markers; and five dedicated-server
runs each exited normally with exactly one synchronous self-test and one clean
shutdown marker.

Those runs used an immutable development runtime built from an uncommitted
source tree. Release promotion still requires the same evidence to be retained
from clean source and a freshly staged final package. Multiplayer evidence must
retain `ui_autoJoin 1` unless the join flow itself is under test.
