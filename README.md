# fty-info
Agent which returns information

## How to build

To build fty-info project run:

```bash
./autogen.sh
./configure
make
make check # to run self-test
```

## Protocols

Connects USER peer to fty-info peer.

The USER peer sends the following message using MAILBOX SEND to 
fty-info peer:
* INFO - request info about the system

The fty-info peer MUST respond with this message back to USER 
peer using MAILBOX SEND:
* uuid in format as defined in RFC-4122
* hostname
* product name
* location
* version
* REST API root path
* REST API port

