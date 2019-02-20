# Wakaama LWM2M example client

This application starts a
[LWM2M](https://wiki.openmobilealliance.org/display/TOOL/What+is+LwM2M) client
on the node with instances of the following objects:
- [Security object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Security-v1_0.xml)
- [Server object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Server-v1_0.xml)
- [Access control object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Access_Control-v1_0_2.xml)
- [Device object](http://www.openmobilealliance.org/tech/profiles/LWM2M_Device-v1_0_3.xml)
- [Light control object](http://www.openmobilealliance.org/tech/profiles/lwm2m/3311.xml)

It will create an instance of the Light control object per LED (LED0 and LED1)
if available. The state of the LED can be changed from the LWM2M server or by
using a shell command in which case the state of it will be reflected on the
LWM2M server.

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

To run the bootstrap server, make sure that the ports it uses are different
from the ones of previous server (default are 5683 for CoAP, 5684 for CoAPs,
and 8080 for the webserver), and that it corresponds to the one set in
`sys/net/lwm2m.h` as `LWM2M_BSSERVER_PORT`:
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
   `sys/net/lwm2m.h` as `LWM2M_DEVICE_NAME`.
3. Using the `LWM2M Server` tab enter the address where the LWM2M server is
   listening. For now only `No security` mode can be used.

### Running the client
The address set in `sys/net/lwm2m.h` as `LWM2M_SERVER_URI` should be reachable
from the node, e.g. either running on native with a tap interface or as a mote
connected to a
[border router](https://github.com/RIOT-OS/RIOT/tree/master/examples/gnrc_border_router).
Also, if a bootstrap server is being used the option `LWM2M_SERVER_IS_BOOTSTRAP`
should be set to 1.

#### Compile and run
To compile run:

```shell
BOARD=<board> make clean all flash term
```

#### Shell commands
This application provides two commands realated to the LWM2M client:
- `lwm2m start`: Starts the LWM2M by configuring the module and registering to
  the server.
- `lwm2m light <num>`: Toggles the state (ON/OFF resource) of the <num> instance
  of the light control object. This should toggle the correspondent LED on the
  board and the change should be reflected in the server.
