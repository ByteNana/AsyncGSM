# MQTT implementation documentation

## MQTT without TLS

### Connecting to the MQTT server

1. Configure Receiving mode:

```
AT+QMTCFG: "recv/mode",<msg_recv_mode>,<msg_len_enable>

<msg_recv_mode>
Integer type. Whether the MQTT message receiving mode.
0: MQTT message received from server is contained in URC.
1: MQTT message received from server is not contained in URC.

<msg_len_enable>
Integer type. Whether the length of MQTT message received from server is contained in URC.
0: Not contained
1: Contained
```

2. Open the network connection to the MQTT server:

```
AT+QMTOPEN=<client_idx>,<host_name>,<port>

<client_idx>
Integer type. MQTT client identifier. Range: 0–5.

<host_name>
String type. The address of the server. It could be an IP address or a domain name. The maximum size is 100 bytes.

<port>
Integer type. The listening port of the server. Range: 1–65535.

Expects:

+QMTOPEN: <client_idx>,<result>
<result>
Integer type. Result of the command execution.
-1: Failed to open network
0: Network opened successfully
1: Wrong parameter
2: MQTT identifier is occupied
3: Failed to activate PDP
4: Failed to parse domain name
5: Network connection error
```

3. Connect to the MQTT server:

```
AT+QMTCONN=<client_idx>,<clientID>[,<username>,<password>]

<client_idx>
Integer type. MQTT client identifier. Range: 0–5.

<clientID>
String type. The client identifier string.

<username>
String type. User name of the client. It can be used for authentication.

<password>
String type. Password corresponding to the user name of the client. It can be used for authentication.

Expects:

+QMTCONN: <client_idx>,<result>[,<ret_code>]

<result>
Integer type. Result of the command execution.
0: Packet sent successfully and ACK received from server
1: Packet retransmission
2: Failed to send packet

<ret_code>
Integer type. MQTT connection state.
1: MQTT is initializing
2: MQTT is connecting
```

### Data Transmission

1. Publish a message to a topic:

```
AT+QMTPUBEX=<client_idx>,<msgid>,<qos>,<retain>,<topic>,<length>

<client_idx>
Integer type. MQTT client identifier. Range: 0–5.

<msgid>
Integer type. Message identifier of packet. Range: 0–65535. It is 0 only when <qos>=0.

<qos>
Integer type. Quality of Service level.
0: At most once
1: At least once
2: Exactly once

<retain>
Integer type. Whether or not the server will retain the message after it has been delivered to the current subscribers

0: The server will not retain the message
1: The server will retain the message

<topic>
String type. Topic that needs to be published.

<length>
Integer type. Length of message to be published.

Expects:

+QMTPUBEX: <client_idx>,<msgid>,<result>[,<value>]

<result>
Integer type. Result of the command execution.
0: Packet sent successfully and ACK received from server - qos 0 does not wait for ACK
1: Packet retransmission
2: Failed to send packet

<value>
Integer type.
If <result> is 1, it means the times of packet retransmission.
If <result> is 0 or 2, it is not presented.

```

2. Subscribe to a topic:

```
AT+QMTSUB=<client_idx>,<msgid>,<topic1>,<qos1>[,<topic2>,<qos2>,...,<topic5>,<qos5>]

<client_idx>
Integer type. MQTT client identifier. Range: 0–5.

<msgid>
Integer type. Message identifier of packet. Range: 0–65535.

<topic>
String type. Topic that needs to be subscribed.

<qos>
Integer type. The QoS level at which the client wants to publish the messages.
0: At most once
1: At least once
2: Exactly once

Expects:
+QMTSUB: <client_idx>,<msgid>,<result>[,<value>]

<result>
Integer type. Result of the command execution.
0: Sent packet successfully and received ACK from server
1: Packet retransmission
2: Failed to send packet

<value>
Integer type.
If <result> is 0, it is a vector of granted QoS levels.
If <result> is 1, it means the times of packet retransmission.
If <result> is 2, it is not presented.
```

3. Unsubscribe from a topic:

```
AT+QMTUNS=<client_idx>,<msgid>,<topic1>[,<topic2>…]

<client_idx>
Integer type. MQTT client identifier. Range: 0–5.

<msgid>
Integer type. Message identifier of packet. Range: 0–65535.

<topic>
String type. Topic that needs to be unsubscribed.

Expects:
+QMTUNS: <client_idx>,<msgid>,<result>[,<value>]

<result>
Integer type. Result of the command execution.
0: Sent packet successfully and received ACK from server
1: Packet retransmission
2: Failed to send packet

<value>
Integer type.
If <result> is 0, it is a vector of granted QoS levels.
If <result> is 1, it means the times of packet retransmission.
If <result> is 2, it is not presented.

````
