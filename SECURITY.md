# Security Policy

## Supported Scope

Security reports are welcome for the actively maintained Qt 6 / CMake codepath in this repository.

Areas most likely to be security-relevant include:

- network input handling
- telemetry parsing
- vendored protocol integrations
- configuration and secret handling
- file writing and local process execution paths

## Reporting a Vulnerability

Please do not open public GitHub issues for unpatched security vulnerabilities.

Instead:

1. prepare a concise report with reproduction details, affected files, and impact
2. include environment details and whether the issue is local-only or remotely triggerable
3. send the report privately to the maintainer through the contact channel documented in [SUPPORT.md](/home/boots/work/phd/GCSNetworkPlanner/SUPPORT.md)

## What to Include

- affected version or commit
- platform and runtime details
- steps to reproduce
- expected vs actual behavior
- proof-of-concept if safe to share
- suggested remediation, if available

## Disclosure Expectations

- allow reasonable time for triage and patching before public disclosure
- avoid publishing exploit details for unpatched issues
- if you are unsure whether something is security-sensitive, report it privately first

## Current Notes

The project still contains legacy vendor code and research-era subsystems. Reports that identify concrete exploitability or unsafe parsing behavior in those areas are especially valuable.

