# Secure implementation documentation

## Connection Security

1. Set the ssl version:

```
AT+QSSLCFG="sslversion",<sslindex>,<sslversion>

<sslindex>
Integer type. SSL context ID. Range: 0–5.
<sslversion>
Integer type. SSL version.
0: SSL 3.0
1: TLS 1.0
2: TLS 1.1
3: TLS 1.2
4: ALL
```

2. Set the cipher suite:

```
AT+QSSLCFG="ciphersuite",<sslindex>,<ciphersuite>

<sslindex>
Integer type. SSL context ID. Range: 0–5.

<ciphersuite>
0: ..
0x0035: TLS_RSA_WITH_AES_256_CBC_SHA
0xFFFF: ALL

```

3. Set the security level:

```
"AT+QSSLCFG="seclevel",<sslindex>[,<seclevel>]

<sslindex>
Integer type. SSL context ID. Range: 0–5.
<seclevel>
Integer type. The authentication mode.
0: No authentication
1: Perform server authentication
2: Perform server and client authentication if requested by the remote
server
```

4. Set certificate:

```
AT+QSSLCFG="cacert",<sslindex>[,<cacertpath>]

<sslindex>
Integer type. SSL context ID. Range: 0–5.
<cacertpath>
String type. The path of the trusted CA certificate.

If the optional parameter is omitted, query the path of
configured trusted CA certificate for the specified SSL context
```

5. Open SSL connection:

```
AT+QSSLOPEN=<ctxindex>,<sslindex>,<clientid>,<serveraddr>,<server_port>[,<access_mode>>]

<ctxindex>
Integer type. PDP context ID. Range: 1–7.
<sslindex>
Integer type. SSL context ID. Range: 0–5.
<clientid>
Integer type. Socket index. Range: 0–11.
<serveraddr>
String type. The address of remote server
<server_port>
Integer type. The listening port of remote server
<access_mode>
0: Buffer access mode
1: Direct push mode
2: Transparent mode
```

## Data Transmission Security

1. Send data over SSL connection:

```
AT+QSSLSEND=<clientid>,<length>
<clientid>
Integer type. Socket index. Range: 0–11.
<length>
Integer type. The length of data to be sent. Range: 1–1460. Unit: byte.
```

2. Close SSL connection:

```
AT+QSSLCLOSE=<clientid>
<clientid>
Integer type. Socket index. Range: 0–11.
```

## Receive Data Indication

When data is received over an SSL connection, the module sends the following URC to indicate the data reception:

```
+QSSLURC: <clientid>,<length>
<clientid>
Integer type. Socket index. Range: 0–11.
<length>
Integer type. The length of received data. Range: 1–1500. Unit: byte.
```
