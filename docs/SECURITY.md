# Security Policy

[English](SECURITY.md) | [简体中文](SECURITY.zh-CN.md)

## Public repository boundary

This repository must not contain production SSIDs, passwords, broker endpoints, device-specific tokens, certificates, private keys, local IP addresses, serial logs, generated `sdkconfig`, build directories, or private deployment history.

`main/secrets.h` is ignored by Git. Start from `main/secrets.h.example`, keep the real file local, and rotate any credential that is accidentally committed before continuing work.

## Deployment recommendations

- Use `mqtts://` with certificate verification.
- Issue distinct credentials and ACLs for each device.
- Limit a device to its own `<device-id>/...` topic namespace.
- Disable anonymous broker access and protect network ports with a firewall.
- Use a separately powered, common-ground lighting supply and do not expose hardware control interfaces directly to the internet.

## Reporting

Do not publish suspected secrets in Issues, pull requests, screenshots, or logs. Use GitHub private security reporting when it is available to the repository owner.
