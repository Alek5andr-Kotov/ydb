## Unreleased

### Bug fixes

* 17122:Fixed a rare issue that caused client applications to hang during commit operations. The problem occurred because the `TEvDeletePartition` message could arrive before the `TEvApproveWriteQuota` message. The batch did not send TEvConsumed and this blocked the queue of write quota requests. [#17122](https://github.com/ydb-platform/ydb/pull/17122) ([Alek5andr-Kotov](https://github.com/Alek5andr-Kotov))
* 16649:Fixed an issue where distconf would issue a VERIFY error and crash when nodes are removed through the legacy Console configuration management interface. [#16649](https://github.com/ydb-platform/ydb/pull/16649) ([Alexander Rutkovsky](https://github.com/alexvru))
* 16635:Fixed an [error](https://github.com/ydb-platform/ydb/issues/15551) that caused the `RETURNING` clause to work incorrectly with `INSERT/UPSERT` operations. [#16635](https://github.com/ydb-platform/ydb/pull/16635) ([Vitalii Gridnev](https://github.com/gridnevvvit))
* 16420:Added `Ydb::StatusIds::TIMEOUT` to the list of retryable errors, which improves the ability to build large secondary indexes. [#16420](https://github.com/ydb-platform/ydb/pull/16420) ([azevaykin](https://github.com/azevaykin))
* 16269:Fixed the issue of a hanging `Drop Tablet` operation in the PQ tablet, caused by the tablet receiving duplicate TEvPersQueue::TEvProposeTransaction messages from SS, especially during delays in IC operation. [#16269](https://github.com/ydb-platform/ydb/pull/16269) ([Alek5andr-Kotov](https://github.com/Alek5andr-Kotov))
* 16194:Fixed a verification failure that occurred during VDisk compaction. [#16194](https://github.com/ydb-platform/ydb/pull/16194) ([Alexander Rutkovsky](https://github.com/alexvru))
* 15570:Allow creation of views that use UDFs in their queries. [#15570](https://github.com/ydb-platform/ydb/pull/15570) ([Daniil Demin](https://github.com/jepett0))
* 15515:Fixed a topic reading hang that occurred when at least one partition had no incoming data but was being read by multiple consumers. [#15515](https://github.com/ydb-platform/ydb/pull/15515) ([FloatingCrowbar](https://github.com/FloatingCrowbar))
* 18117:Moved changes from: https://github.com/ydb-platform/ydb/pull/18079

Issue: https://github.com/ydb-platform/ydb/issues/18116

This PR fixes optimisation in `NKikimr::NRawSocket::TBufferedWriter`, that wrote the entire message directly to socket if message was larger than available space in buffer. When we wrote more than 6mb to socket with SSL enabled, it every time returned -11 (WAGAIN). 
As a quick fix, we replace sending of an entire message to socket with cutting this message into 1mb chunks and sending them to socket one by one. [#18117](https://github.com/ydb-platform/ydb/pull/18117) ([Andrey Serebryanskiy](https://github.com/a-serebryanskiy))
* 18077:Changes from #18072

Issue #18071

The metric value is reset to zero when the `TEvPQ::TEvPartitionCounters` event arrives.

Added a re-calculation of the values. [#18077](https://github.com/ydb-platform/ydb/pull/18077) ([Alek5andr-Kotov](https://github.com/Alek5andr-Kotov))
* 18007:Moved changes from #17913 

Issue #17915

The PQ tablet forgets about the transaction only after it receives a TEvReadSetAck from all participants. Another shard may be deleted before the PQ completes the transaction (for example, due to a split table). As a result, transactions are executed, but remain in the WAIT_RS_ACKS state.

If the PQ tablet sends a TEvReadSet to a tablet that has already been deleted, it receives a TEvClientConnected with the `Dead` flag in response. In this case, we consider that we have received a TEvReadSetAck. [#18007](https://github.com/ydb-platform/ydb/pull/18007) ([Alek5andr-Kotov](https://github.com/Alek5andr-Kotov))

