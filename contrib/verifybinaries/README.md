### Verify Binaries

#### Preparation:

Once BurritoCoin Core publishes its first signed release, the canonical
release-signing key fingerprint will be added to
[`contrib/gitian-keys/keys.txt`](../gitian-keys/keys.txt) and listed on the
GitHub Releases page. Until that release exists, no canonical signing key
is published — anyone claiming to sign BurritoCoin Core releases right now
should not be trusted.

When the key is published, fetch it and verify the fingerprint against the
copy on the GitHub Releases page (and any other authoritative location):

```sh
gpg --fingerprint "<future fingerprint>"
```

#### Usage:

This script attempts to download a release-checksum signature
(`SHA256SUMS.asc`) from a release manifest URL and verify both the
signature and the per-file hashes.

It returns 0 if everything passes, 1 if either check fails, and 2 on any
unexpected error.

```sh
./verify.sh burritocoin-core-0.1.0
./verify.sh burritocoin-core-0.1.0-linux
./verify.sh burritocoin-core-0.1.0-win64
./verify.sh burritocoin-core-0.1.0-osx
```

If you don't want to keep the downloaded binaries, pass any value as the
second argument:

```sh
./verify.sh burritocoin-core-0.1.0 delete
```
