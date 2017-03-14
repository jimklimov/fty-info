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
* [frame 1] INFO - request info about the system
* [frame 1] zuuid - user request uuid

The fty-info peer MUST respond with this message back to USER 
peer using MAILBOX SEND:
* [frame 1] zuuid - original user request uuid
* [frame 2] hash  - hash table decribed bellow

the hash table contains a list of key/value pair.
keys are defined in include/fty_info.h :
* INFO_UUID = "uuid"
* INFO_HOSTNAME = "hostname"
* INFO_NAME = "name"
* INFO_PRODUCT_NAME = "product_name"
* INFO_LOCATION = "location"
* INFO_VERSION = "version"
* INFO_REST_ROOT = "rest_root"
* INFO_REST_PORT = "rest_port"

## How to request fty-info ?
```
    #include "fty_info.h"

    zmsg_t *command = zmsg_new ();
    zmsg_addstrf (command, "%s", FTY_INFO_CMD);
    zmsg_addstrf (command, "%s", zuuid);
    mlm_client_sendto (client, FTY_INFO_AGENT, "my-client-name", NULL, 1000, &command);
```

## How to decode reply from fty-info ?
```
    msg_t *recv = mlm_client_recv (client);
    char *zuuid_reply = zmsg_popstr (recv);
    zframe_t *frame_infos = zmsg_next (recv);
    zhash_t *infos = zhash_unpack(frame_infos); 
    char * product_name = (char *) zhash_lookup (infos, INFO_PRODUCT_NAME );
    zhash_destroy(&infos);
```

