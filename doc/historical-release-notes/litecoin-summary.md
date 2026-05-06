Upstream Litecoin 0.21.4 Release Notes (historical)
===================================================

> **Note:** This document is the verbatim Litecoin Core 0.21.4 release notes,
> kept for historical reference of the upstream codebase BurritoCoin was
> forked from. The download URL and issue tracker referenced below point at
> the upstream project. BurritoCoin Core releases are tracked on
> <https://github.com/burritocoindev/burritocoin/releases>.

This is a new patch version release that includes new features and important security updates.

Notable changes
===============

Important Security Updates
--------------------------

This release contains fixes for the following security vulnerabilities:

- [CVE-2024-35202](https://www.cvedetails.com/cve/CVE-2024-35202/),
which allows remote attackers to cause a denial of service (blocktxn message-handling assertion and node exit)
by including transactions in a blocktxn message that are not committed to in a block's merkle root.
FillBlock can be called twice for one PartiallyDownloadedBlock instance.
  - `5d4a2e5`: backported from BurritoCoin Core (`a8897f6`)

- [Hindered block propagation due to mutated blocks](https://bitcoincore.org/en/2024/10/08/disclose-mutated-blocks-hindering-propagation/),
where a peer could send mutated blocks which could clear the download state of other peers that also announced block, hindering block propagation.
  - `dab3bb7`: backported from BurritoCoin Core (`dbfc748`)

- [Infinite loop bug in miniupnp dependency](https://bitcoincore.org/en/2024/07/31/disclose-upnp-oom/),
which could be exploited by an attacker on the local network to trigger an OOM.
  - `16ba8b8`: backported from BurritoCoin Core (`fa2a5b8`)

Bug fixes
---------
- `0d04e75`: default -peerblockfilters and -blockfilterindex to off when pruning is enabled

Test related fixes
------------------
- `7d9fea0`: fix functional tests that were broken by changes in 0.21.3

Credits
=======

Thanks to everyone who directly contributed to this release:

- [The BurritoCoin Core Developers](https://github.com/bitcoin/bitcoin/)
- [David Burkett](https://github.com/DavidBurkett/)
- [Hector Chu](https://github.com/hectorchu)
- [Loshan](https://github.com/losh11)