# Security Policy

## Reporting a Vulnerability

If you've found a security issue in BurritoCoin Core, please do **not** open
a public GitHub issue. Public disclosure of an unfixed vulnerability puts
every node and wallet on the network at risk.

### Preferred channel

Use GitHub's private vulnerability disclosure feature:

> https://github.com/BurritoCoinDev/BurritoCoin/security/advisories/new

This routes the report directly to the maintainers and keeps the discussion
confidential while we work on a fix.

### Fallback

If GitHub Security Advisories isn't available to you, open a private channel
by emailing the project — see the project README for the current contact.
Mark the subject with `[SECURITY]` so it doesn't get filed with general
inquiries.

## Disclosure timeline

We aim to acknowledge security reports within 72 hours and to ship a fix or
mitigation within 30 days, depending on severity. Coordinated disclosure with
the reporter is the default; we'll publish details only after a fix is
released and operators have had time to upgrade.

## PGP key

A dedicated security PGP key will be published here once the project's
release-signing infrastructure is in place. Until then, the GitHub Security
Advisories channel is the recommended path — its transport is already
end-to-end encrypted between you and the project maintainers.

## Scope

This policy covers the BurritoCoin Core daemon (`burritocoind`), CLI tools
(`burritocoin-cli`, `burritocoin-tx`, `burritocoin-wallet`), and Qt wallet
(`burritocoin-qt`) at the current release tag and `master` branch. It does
not cover third-party wallets, exchanges, or block explorers — please
report issues there to the operators of those services.
