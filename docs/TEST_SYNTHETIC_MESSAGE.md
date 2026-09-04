# TEST/SYNTHETIC message

The `Filters -> Send TEST/SYNTHETIC message...` command is an explicit local smoke-test entry point for the existing PDW filter/action pipeline.

It injects one bounded POCSAG test message (`1234567`, `PDW TEST MESSAGE`) through the same legacy `ShowMessage()` path used after decoded-message projection. The message is marked `TEST` / `SYNTHETIC` so it is distinguishable from received RF traffic.

The command is never automatic. Injection fails closed while FLEX group-call conversion is active, and validation/projection failures leave the legacy message globals untouched.

This is intentionally not Scenario Test mode and does not add network, Main/Slave, multi-receiver, remote-forwarding or update behavior.
