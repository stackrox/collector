# GRPC Communication Layer

The GRPC layer manages bidirectional streaming connections between collector and sensor, handling TLS configuration, channel lifecycle, and message transmission with automatic reconnection.

## Components

### Channel Creation and Configuration

GRPC.cpp:CreateChannel establishes channels with keepalive parameters for long-lived connections. The implementation sets HTTP2 BDP probing, ping intervals (10s keepalive time, 5s minimum receive interval), and allows pings without active calls to maintain connectivity. TLSCredentialsFromConfig reads PEM files from TlsConfig paths and constructs grpc::SslCredentials for mTLS connections. CheckGrpcServer validates server addresses in host:port format.

### Bidirectional Streaming

DuplexGRPC.h defines templates for async bidirectional GRPC streams without explicit multithreading or completion queue manipulation. The abstraction separates three usage patterns: full bidirectional control (DuplexClientReaderWriter), write-only with callback-based reads (CreateWithReadCallback), and write-only ignoring reads (CreateWithReadsIgnored).

DuplexClient manages a grpc::CompletionQueue and tracks operation states through flags. Each async operation (READ, WRITE, WRITES_DONE, FINISH, SHUTDOWN) has pending and done bits in a uint32_t flags field. Operations map to completion queue tags via OpToTag/TagToOp pointer encoding. ProcessSingle polls the completion queue, updates flags, and dispatches event handlers. Synchronous methods like Write and Finish wrap async operations with deadline-based polling.

DuplexClientReaderWriter combines reading and writing. Read messages buffer in read_buf_, with read_buf_valid tracking success. After processing each read, ReadNext immediately issues another async read to maintain the stream. Write operations block until the completion queue indicates success. The destructor calls Shutdown and drains remaining events to ensure clean termination.

StdoutDuplexClientWriter provides a no-op implementation for testing and offline operation. All methods return success immediately, and Write calls LogProtobufMessage to emit protobufs to stdout.

### Signal Service Client

SignalServiceClient manages a persistent GRPC stream for process signals. The implementation runs EstablishGRPCStream in a StoppableThread, which repeatedly calls EstablishGRPCStreamSingle to create connections with automatic retry. WaitForChannelReady (from GRPCUtil) polls channel state with 1-second intervals, checking for GRPC_CHANNEL_SHUTDOWN as a fatal condition.

Stream lifecycle: SignalServiceClient:30 creates the DuplexClient writer via CreateWithReadsIgnored since sensor doesn't send data on this stream. WaitUntilStarted blocks up to 30 seconds for stream readiness. The first_write_ flag triggers NEEDS_REFRESH to force a full process list resend after reconnection. Write failures invoke FinishNow, reset the writer, and set stream_active_ to false, triggering the background thread to reconnect.

StoppableThread uses a pipe and condition variable for cancellation. The stream_interrupted_ condition variable wakes EstablishGRPCStream when stream_active_ transitions to false, minimizing reconnection latency.

## Configuration

CollectorConfig stores grpc_server_ addresses and tls_config_ objects. InitCollectorConfig processes command-line arguments via CollectorArgs or environment variables (GRPC_SERVER, ROX_COLLECTOR_TLS_*). TlsConfig validates that CA, client cert, and client key paths are all non-empty via IsValid.

GRPC.cpp:24 reads complete file contents into SslCredentialsOptions strings. This approach loads certificates once at startup rather than maintaining file handles.

## Connection Management

GRPCUtil:WaitForChannelReady polls grpc::Channel::WaitForConnected with a timeout, checking an interrupt function between attempts. This allows graceful shutdown during connection establishment. The collector main loop passes lambdas that check control flags and signal state.

SignalServiceClient maintains connection state with atomic stream_active_ and synchronizes reconnection through stream_interrupted_. When PushSignals detects a write failure, it notifies the background thread via stream_interrupted_.notify_one(), which immediately attempts reconnection without waiting for the next scheduled retry.

## Error Handling

CheckGrpcServer returns ARG_OK/ARG_ILLEGAL/ARG_IGNORE with error messages. Invalid formats (missing host/port, malformed address, length >255) return ARG_ILLEGAL. Missing configuration returns ARG_IGNORE with a message about reverting to stdout.

DuplexClient tracks STREAM_ERROR via flag bit to propagate failures. When ok=false in completion queue events, ProcessEvent sets STREAM_ERROR and issues FinishAsync. Sync operations check Result::ok() and Result::IsTimeout() to distinguish success, errors, and timeouts.

SignalServiceClient:PushSignals throttles error logs to once per 10 seconds when the stream is unavailable, preventing log spam during extended sensor outages. Write failures capture the grpc::Status error message before resetting the writer.

## Integration Points

CollectorService creates the grpc::Channel via CreateChannel and stores it in config_.grpc_channel for sharing between components. NetworkStatusNotifier and other components receive the channel to create their own GRPC clients. WaitForGRPCServer blocks during startup until the channel reaches READY state, ensuring sensor connectivity before beginning data collection.

StdoutSignalServiceClient provides the same interface without GRPC dependencies, allowing collector to run without sensor by emitting signals as JSON to stdout. This supports debugging and offline analysis workflows.
