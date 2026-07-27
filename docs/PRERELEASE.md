# Release Checklist

Use this checklist for every published SHAYVERI release. Commands use
`v2.9.0` as an example.

## 1. Finalize the engine

- [ ] Choose the version according to [`VERSIONING.md`](VERSIONING.md).
- [ ] Add the new version's number to the GitHub actions workflow.
- [ ] Freeze the release scope, production defaults, and default NNUE.
- [ ] Remove temporary gates and unfinished experimental behaviour.
- [ ] Confirm tuning-only options are absent from the production UCI handshake.

## 2. Build and verify

- [ ] Run tests and sanitize suite (LSAN unavailable).
```sh
make -j23
make test -j23
make -C debug ASAN=1 UBSAN=1 LSAN=0 sanitize -j23
printf 'uci\nisready\nquit\n' | ./SHAYVERI
printf 'bench 16 1 3 default depth\nquit\n' | ./SHAYVERI
```

- [ ] Confirm repeated benches produce the same node signature.
- [ ] If an intentional search change altered the signature, update
  [`debug/uci_check.py`](../debug/uci_check.py) and
  [`scripts/profiling/bench.sh`](../scripts/profiling/bench.sh).

## 3. Validate strength

- [ ] Run a 1,000-game colour-paired match against the previous public release
  at `5+0.05`, one thread, and 64 MB hash.
- [ ] Run the anchored STC and LTC release pin exactly as described in
  [`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md).

Start the master:

```sh
cd ~/elo_pin
PIN_ROOT="$PWD" RELEASE_ID=v2.9.0 \
  NET= REGISTER_SECONDS=300 ./master.sh
```

Start one worker on each participating machine:

```sh
cd ~/elo_pin
PIN_ROOT="$PWD" ./worker.sh
```

Monitor the pin:

```sh
cd ~/elo_pin
PIN_ROOT="$PWD" ./watch.sh
```

- [ ] Confirm both phases completed successfully.

## 4. Finalize documentation

- [ ] Ensure [`CHANGELOG.md`](CHANGELOG.md) describes every engine change.
- [ ] Remove `(Unreleased)`, candidate ranges, and pending-validation wording.
- [ ] Update [`README.md`](../README.md), [`ELO.md`](ELO.md), and
  [`index.html`](../index.html) with the release and applicable results.
- [ ] Remove obsolete internal names, paths, versions, and milestone wording.

If the default NNUE changed:

- [ ] Update the [`Makefile`](../Makefile) and engine integration.
- [ ] Update the network cases in
  [`build-release.yml`](../.github/workflows/build-release.yml).
- [ ] Update the mapping in
  [`write-release-readme.sh`](../.github/scripts/write-release-readme.sh).

## 5. Merge and tag

- [ ] Review the complete diff from the previous public tag.
- [ ] Rebuild and repeat affected gates after any engine or build-input change.
- [ ] Commit the finalized release, merge it into `main`, and confirm the
  working tree is clean.

```sh
git tag -a v2.9.0 -m "SHAYVERI v2.9.0"
git push origin main
git push origin v2.9.0
```

## 6. Build and publish

The tag must be pushed before running the release workflow.

```sh
gh workflow run build-release.yml \
  --repo shahyaranfaz/SHAYVERI \
  --ref main \
  -f tag=v2.9.0 \
  -f publish=true
```

- [ ] Confirm every build and publish job succeeds.

## 7. Verify the release

- [ ] Inspect the release title, notes, and assets.
- [ ] Test at least one downloaded package outside the source tree.
