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

## Uploading Certificates

To use server authentication (`seclevel=1`), the module must have a CA certificate stored in UFS and the SSL context must point to it via `QSSLCFG="cacert"`.

Upload file with `QFUPL` (AT-only example):

```
AT+QFUPL="<filename>",<size>,<timeout>
CONNECT
<send <size> bytes of the PEM file>
OK
+QFUPL: <size>
```

Then set the SSL context to use the uploaded file and enable verification:

```
AT+QSSLCFG="cacert",<sslindex>,"<filename>"
AT+QSSLCFG="seclevel",<sslindex>,1
```

## Library Usage (MD5-Named CA)

- `AsyncSecureGSM::setCACert(const char* pem)`: Calculates MD5 of the provided PEM and uses the 32-char hex digest as the UFS filename.
  - If the MD5 matches the last applied value (cached), it does nothing.
  - Otherwise it checks UFS for an existing file with that MD5 name using `QFLST`.
    - If found, it reuses it.
    - If not found, it uploads the PEM under that MD5 filename.
  - Finally it sets `QSSLCFG="cacert"` to that filename and sets `seclevel=1`.

Helpers exposed by the modem driver:

- `findUFSFile(pattern, &name, &size)`: Returns true if at least one match exists from `AT+QFLST="pattern"`, and optionally returns the first match and size.
- `uploadUFSFile(path, data, size)`: Uploads raw bytes to UFS using `AT+QFUPL`.
- `setCACertificate(path)`: Sets `QSSLCFG="cacert"` and `seclevel=1` for the active SSL index.

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
