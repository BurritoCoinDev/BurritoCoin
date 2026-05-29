# Seeds

Utility to generate the seeds.txt list that is compiled into the client
(see [src/chainparamsseeds.h](/src/chainparamsseeds.h) and other utilities in [contrib/seeds](/contrib/seeds)).

Be sure to update `PATTERN_AGENT` in `makeseeds.py` to include the current version,
and remove old versions as necessary (at a minimum when GetDesirableServiceFlags
changes its default return value, as those are the services which seeds are added
to addrman with).

The seeds compiled into the release are created from pool operators' DNS seed
data. Once a public pool exists, the workflow looks like this (replace
`pool.example.com` with the actual pool host):

    curl -s https://pool.example.com/seeds.txt > seeds_main.txt
    python3 makeseeds.py < seeds_main.txt > nodes_main.txt
    python3 generate-seeds.py . > ../../src/chainparamsseeds.h

Until a public pool exists, edit `contrib/seeds/nodes_main.txt` and
`nodes_test.txt` by hand with known-good peers and run
`generate-seeds.py` directly.

## Dependencies

Ubuntu:

    sudo apt-get install python3-dnspython
