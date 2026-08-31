# Security policy

## Supported code

Security fixes target the latest ArenaMP source revision. Older client/server combinations are not supported and should not be exposed publicly.

## Reporting a vulnerability

Use the repository's private GitHub Security Advisory feature when available. Do not publish working exploits, credentials, private server data, or unredacted crash dumps before maintainers have had time to investigate.

Include the affected revision, platform, attack prerequisites, impact, reproduction steps, and a minimal proof of concept. State whether the issue crosses the client/server trust boundary or permits persistent world/player-data modification.

## Server operator guidance

- Keep client and server on the same trusted revision.
- Back up `server/data/` before upgrading.
- Restrict filesystem and network permissions of the server process.
- Review custom Lua scripts before deployment.
- Enable required-data-file enforcement for untrusted public clients after generating and testing the correct manifest.
- Do not expose administration credentials or private player JSON in logs or bug reports.

See [server/COREARENAMP_SECURITY.md](server/COREARENAMP_SECURITY.md) for implementation-specific notes.

