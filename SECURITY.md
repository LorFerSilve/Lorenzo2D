# Security Policy

## Supported versions

Security fixes are applied to the current `master` branch and, when applicable, the latest
published release. Older development branches are not supported security targets.

## Reporting a vulnerability

Please do not open a public issue for a vulnerability that could put users, credentials, or
systems at risk.

Use GitHub's private vulnerability reporting for this repository when it is available. If
private reporting is unavailable, contact the repository owner privately through their GitHub
profile and provide:

- the affected Lorenzo2D version or commit;
- a minimal reproduction;
- the expected security impact;
- any known workaround.

Do not include real credentials, personal data, or third-party secrets in a report.

## Secrets

Lorenzo2D does not require repository secrets for its normal CI workflow. Never commit API
keys, access tokens, private keys, environment files, signing certificates, or credential
stores. If a secret is committed accidentally, revoke or rotate it immediately; deleting the
file in a later commit is not sufficient because Git history is retained.
