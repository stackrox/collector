Collector
=========

Welcome to **Collector**!!

**Collector** is a system visibility docker container.

#### Kernel module configurability

The Collector kernel module monitors system calls from every container and every process and turns them into events that are sent either to stdout or to a file. We enable configuring the set of system calls that the module should monitor by providing a whitelist of system calls. The whitelist can be specified in a JSON formatted configuration using the following format:

~~~
{ "syscalls": ["open", "close", "read", "write"] }
~~~

#### Event formatter configurability

We enable configuring the event format that is emitted by the kernel module. The configuration specifies the sequence of fields that need to be emitted together with the label for each field. The latter enables us to override field names. The order in which fields should be emitted, the event format can be specified in a JSON formatted configuration using the following format:

~~~
{ "--output-format":"container:container.id,event:evt.type,time:evt.time,rawtime:evt.rawtime,direction:evt.dir,image:container.image,name:container.name" }
~~~

#### Using Kafka

Collector enables sending events using Kafka. The functionality is enabled using the following configuration option:

~~~
{ "--use-kafka, --default-topic:"topic-signals", --network-topic:"topic-network", --process-topic:"rox-process-signals" }
~~~

The broker list for Kafka based event production is expected to be in an environment variable covered in the section on environment variables.

#### Chisel support

The Collector supports a single chisel. The chisel is expected to be base64 encoded and passed through an environment variable called CHISEL.

#### Environment variables

BROKER_LIST is required to send formatted events over Kafka. There is no default.

CHISEL is expected to contain a base64 encoded chisel file if the Collector should have additional configuration using a chisel. By defult the Collector operates with no chisel.

SYSDIG_HOST_ROOT is required by the kernel module; used as a prefix to paths like /proc, and /boot. Make sure this matches the volume mount paths used when launching this container.

COLLECTOR_CONFIG is the required json config string. A default Collector config that can be used to bring up the Collector and generate events to stdout is as follows:

~~~
{"syscalls":["open","close","read","write"],"output":"stdout","format":"container:container.id,event:evt.type,time:evt.time,rawtime:evt.rawtime,direction:evt.dir,image:container.image,name:container.name"}
~~~

MAX_CONTENT_LENGTH_KB is optional, and it defaults to 1024.

CONNECTION_LIMIT is optional, and it defaults to 64.

CONNECTION_LIMIT_PER_IP is optional, and it defaults to 64.

CONNECTION_TIMEOUT is optional, and it defaults to 8.

#### Running Collector with default configuration

~~~
docker run --name collector -d --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /dev:/host/dev -v /proc:/host/proc:ro -v /boot:/host/boot:ro -v /lib/modules:/host/lib/modules:ro -v /usr:/host/usr:ro -e COLLECTOR_CONFIG='{"syscalls":["open","close","read","write"],"output":"stdout","format":"container:container.id,event:evt.type,time:evt.time,rawtime:evt.rawtime,direction:evt.dir,image:container.image,name:container.name"}' collector
~~~
