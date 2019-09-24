# Wakaama LWM2M example client

This application starts a
[LWM2M](https://wiki.openmobilealliance.org/display/TOOL/What+is+LwM2M) client
on the node with instances of the following objects:
- [Security object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Security-v1_0.xml)
- [Server object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Server-v1_0.xml)
- [Access control object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Access_Control-v1_0_2.xml)
- [Device object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Device-v1_0_3.xml)

The application is based on the Eclipse Wakaama
[example client](https://github.com/eclipse/wakaama/tree/master/examples/client)
.

## Usage

### Setting up a LWM2M Test Server
To test the client a LWM2M server where to register is needed.
[Eclipse Leshan](https://github.com/eclipse/leshan) demo is a good option for
running one locally.

To run the demo server:
```shell
wget https://hudson.eclipse.org/leshan/job/leshan/lastSuccessfulBuild/artifact/leshan-server-demo.jar

java -jar ./leshan-server-demo.jar
```
It will output the addresses where it is listening:
```
INFO LeshanServer - LWM2M server started at coap://0.0.0.0/0.0.0.0:5683 coaps://0.0.0.0/0.0.0.0:5684
INFO LeshanServerDemo - Web server started at http://0.0.0.0:8080/.
```

#### Bootstrap server
LWM2M provides a bootstrapping mechanism to provide the clients with information
to register to one or more servers. To test this mechanism both the previous server and a bootstrap server should be running. Eclipse Leshan also provides a bootstrap server demo.

By default the bootstrap server option is disabled, it can be enabled by setting
`LWM2M_SERVER_IS_BOOTSTRAP` in `lwm2m.h` to 1.

To run the bootstrap server, make sure that the ports it uses are different
from the ones of previous server (default are 5683 for CoAP, 5684 for CoAPs,
and 8080 for the webserver), and that it corresponds to the one set in
`lwm2m.h` as `LWM2M_BSSERVER_PORT`:
```shell
# download demo
wget https://hudson.eclipse.org/leshan/job/leshan/lastSuccessfulBuild/artifact/leshan-bsserver-demo.jar

# set CoAP, CoAPs and webserver ports for bootstrap server
BS_COAPPORT=5685
BS_COAPSPORT=5686
BS_WEBPORT=8888

# run the server
java -jar ./leshan-bsserver-demo.jar --coapport ${BS_COAPPORT} \
            --coapsport ${BS_COAPSPORT} --webport ${BS_WEBPORT}
```

To set up the configuration of the node and the server:
1. Click the `Add new client bootstrap configuration` button.
2. Fill in the name of the device, it **should** match the one set in
   `lwm2m.h` as `LWM2M_DEVICE_NAME`.
3. Using the `LWM2M Server` tab enter the address where the LWM2M server is
   listening. For now only `No security` mode can be used.

### Running the client
The address set in `lwm2m.h` as `LWM2M_SERVER_URI` should be reachable
from the node, e.g. either running on native with a tap interface or as a mote
connected to a
[border router](https://github.com/RIOT-OS/RIOT/tree/master/examples/gnrc_border_router).

Also, if a bootstrap server is being used the option `LWM2M_SERVER_IS_BOOTSTRAP`
should be set to 1.

The server URI for the example is being defined using the variable `SERVER_URI`
in the Makefile, and can be changed when compiling.

#### Compile and run
For debugging purposes there are two types of messages that can be enabled:
- The lwm2m client adaption debug can be enabled by setting `ENABLE_DEBUG` in
  `lwm2m_client.c` to 1
- The wakaama internal logging can be enabled by adding `LWM2M_WITH_LOGS` to the
  CFLAGS (`CFLAGS += -DLWM2M_WITH_LOGS`)

To compile run:

```shell
BOARD=<board> make clean all flash term
```

#### Shell commands
- `lwm2m start`: Starts the LWM2M by configuring the module and registering to
  the server.
