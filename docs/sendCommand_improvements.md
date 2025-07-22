# Proposal: Enhanced `sendCommand` API

The current `AsyncATHandler::sendCommand` method expects a fully constructed `String` containing the AT command. When building commands with parameters this forces callers to allocate and concatenate strings manually:

```cpp
String cmd = String("AT+QICSGP=1,1,\"") + apn + "\",""" + (user ? user : "") + "\",""" + (pwd ? pwd : "") + "\"";
if (!at.sendCommand(cmd, response, "OK", 1000)) {
    return false;
}
```

This approach is error‑prone and leads to many temporary allocations. TinyGSM solves this by providing a `sendAT` helper that accepts a variable number of parts which are concatenated internally before sending. We could adopt a similar style by adding an overload or helper to `AsyncATHandler`.

## Suggested API

```cpp
// Variadic helper that assembles the command from multiple pieces
template <typename... Args>
bool sendCommand(String &response, const char *expected, uint32_t timeoutMs,
                 Args&&... parts);
```

Usage would then become:

```cpp
at.sendCommand(response, "OK", 1000,
               "AT+QICSGP=1,1,\"", apn, "\",\"", user, "\",\"", pwd, "\"");
```

Internally the function could pre‑reserve enough space and append each part sequentially. This reduces the need for intermediate `String` objects and keeps call sites concise.

An alternative is a `printf`‑style method:

```cpp
bool sendCommandf(String &response, const char *expected, uint32_t timeoutMs,
                  const char *fmt, ...);
```

While familiar, the printf approach may increase flash size on embedded targets. The variadic version keeps type safety and allows mixing `const char *`, `String`, or numeric types (converted with `String(part)`).

## Benefits
- Cleaner call sites without manual concatenation
- Fewer temporary allocations
- More flexible and easier to extend with additional parameters

Further discussion is needed on whether the helper should handle PROGMEM strings or be constexpr‑friendly, but adding a basic variadic overload would already improve usability.
