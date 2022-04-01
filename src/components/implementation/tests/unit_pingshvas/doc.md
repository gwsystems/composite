## tests

Basic ping pong test for shared namespaces. Includes a call to `pongshvas_send` which has the server send the value of a shared variable to the client (useful for testing if the alias functionality works) and `pongshvas_rcv_and_update`, which has the client send a pointer variable to the server, which the server then deferences and updates (useful for testing with servers and clients that are in the same shared namespace)

### Description

### Usage and Assumptions
