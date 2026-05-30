# Security Policy

## Supported Versions

Security fixes are handled on the current main development line unless a release
branch is explicitly maintained.

## Reporting a Vulnerability

Please report security issues privately to the project maintainers instead of
opening a public issue. Include:

- A short description of the issue
- A minimal reproducer or affected code path when possible
- Expected impact
- Any relevant compiler, platform, or protocol version details

The maintainers will review the report, coordinate a fix, and publish details
once users have had a reasonable chance to update.

## Scope

KProtocol is a protocol library and lightweight server runtime. Security reports
that involve packet parsing, compression limits, malformed network input,
resource exhaustion, login helpers, or installed package behavior are in scope.
