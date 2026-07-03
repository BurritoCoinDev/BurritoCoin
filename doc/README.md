BurritoCoin Core
=============

Setup
---------------------
BurritoCoin Core is the original BurritoCoin client and it builds the backbone of the network. It downloads and, by default, stores the entire history of BurritoCoin transactions, which requires approximately 22 gigabytes of disk space. Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

To download BurritoCoin Core, visit [burritoco.in](https://burritoco.in/).

Running
---------------------
The following are some helpful notes on how to run BurritoCoin Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/burritocoin-qt` (GUI) or
- `bin/burritocoind` (headless)

### Windows

Unpack the files into a directory, and then run burritocoin-qt.exe.

### macOS

Drag BurritoCoin Core to your applications folder, and then run BurritoCoin Core.

### Need Help?

* See the project website at [burritoco.in](https://burritoco.in/) for help and more information.
* File issues or ask questions on the [BurritoCoin GitHub repository](https://github.com/BurritoCoinDev/BurritoCoin/issues).

Building
---------------------
The following are developer notes on how to build BurritoCoin Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [FreeBSD Build Notes](build-freebsd.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The BurritoCoin repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Productivity Notes](productivity.md)
- [Release Notes](release-notes/) (archive of tagged-release notes; empty until v0.1.0)
- [Historical Release Notes (upstream Bitcoin / Litecoin)](historical-release-notes/)
- [Release Process](release-process.md)
- [Source Code Documentation (upstream Bitcoin Core — reference only; BurritoCoin's source layout is identical)](https://doxygen.bitcoincore.org/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [JSON-RPC Interface](JSON-RPC-interface.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* General development discussion happens in
  [GitHub Issues](https://github.com/BurritoCoinDev/BurritoCoin/issues) and
  [Pull Requests](https://github.com/BurritoCoinDev/BurritoCoin/pulls).
  Dedicated chat / forum channels will be announced once the project has the
  contributor base to justify them.

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [burritocoin.conf Configuration File](burritocoin-conf.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Memory](reduce-memory.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)
- [PSBT support](psbt.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
