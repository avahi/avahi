# Security Policy

## Supported Versions

Security updates are only provided for last released version.
Previous versions are not fixed.

| Version | Supported          |
| ------- | ------------------ |
| 0.8.x   | :white_check_mark: |
| < 0.8.0 | :x:                |

## Reporting a Vulnerability

If any can prevent avahi-daemon from responding or even terminating it
Use this section to tell people how to report a vulnerability.
If you discover a way to crash avahi-daemon via network packet or make it non-responsive,
please do not create normal public issue. Instead report it on
[Security](https://github.com/avahi/avahi/security) page as a vulnerability.
Give us enough time to look into it, please do not post it on public places.
Including our own issue tracker or mailing list.
If it does not crash but stops responding in reaction to remote event, report it the same way.

If any tool crashes in reaction to network traffic, consider reporting it the same way.

Crash caused by wrong configuration or when it requires local administrator privileges should be reported as
an ordinary issue only.
