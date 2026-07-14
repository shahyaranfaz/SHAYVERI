# Release Checklist

Use this checklist for every published SHAYVERI release. Version-specific
experiments, tuning, and promotion criteria belong in that release's own plan.
Commands below use `v2.7.0` as the example. Substitute the release being made.

## 1. Freeze the release candidate

- [ ] Choose the version according to [`VERSIONING.md`](VERSIONING.md).
- [ ] Confirm the release scope and final production defaults.
- [ ] Complete the correctness, strength, and performance validation relevant
  to the changes in this release.
- [ ] Remove temporary gates and disable unfinished experimental behavior.
- [ ] Preserve the final binary and the match or benchmark evidence used to
  approve it.
- [ ] Record the release commit, compiler, build flags, bench signature, and
  SHA-256 hashes for the final binary, default NNUE, opening book, opponent
  binaries, anchors, and Ordo executable.
- [ ] Confirm tuning-only registry options are absent from the production UCI
  handshake.

## 2. Record the bench and run verification

- [ ] Build the finalized candidate.

  ```sh
  make -j"$(nproc)"
  ```

- [ ] Run the bench repeatedly and confirm the node signature is deterministic.

  ```sh
  printf 'bench 16 1 3 default depth\nquit\n' | ./SHAYVERI
  ```

- [ ] If intentional search changes altered the signature, confirm the change
  is expected and update both values before running `make test`:
  - `BENCH_SIGNATURE_NODES` in
    [`debug/uci_check.py`](../debug/uci_check.py)
  - `EXPECTED_NODES` in
    [`scripts/profiling/bench.sh`](../scripts/profiling/bench.sh)
- [ ] Run the normal verification suite.

  ```sh
  make test
  ```

- [ ] Run the sanitizer suite.

  ```sh
  make -C debug sanitize
  ```

- [ ] Inspect the production UCI handshake.

  ```sh
  printf 'uci\nisready\nquit\n' | ./SHAYVERI
  ```

- [ ] Confirm every exposed production option is intentional and documented
  consistently in [`README.md`](../README.md) and
  [`index.html`](../index.html).

## 3. Run mandatory release strength testing

- [ ] Use the exact finalized binary for every release comparison.
- [ ] Run a 1,000-game paired match against the previous public release at
  `5+0.05`, one thread, and 64 MB hash.

  ```sh
  set -o pipefail
  FASTCHESS="$HOME/chess_arena/fastchess/fastchess"
  CANDIDATE=./SHAYVERI
  BASELINE=./SHAYVERI_BASELINE
  BOOK=./8moves_GM_LB.epd
  TC=5+0.05
  ROUNDS=500
  CONCURRENCY=23

  "$FASTCHESS" \
    -engine name=candidate cmd="$CANDIDATE" dir=. proto=uci \
      option.OwnBook=false option.Book_Info_Depth=0 \
    -engine name=baseline cmd="$BASELINE" dir=. proto=uci \
      option.OwnBook=false \
    -each proto=uci "tc=$TC" timemargin=100 option.Threads=1 option.Hash=64 \
    -openings file="$BOOK" format=epd order=random plies=16 \
    -games 2 -rounds "$ROUNDS" -repeat \
    -concurrency "$CONCURRENCY" -recover \
    -output format=cutechess \
    -pgnout file=release_h2h.pgn min=true \
    -ratinginterval 0 2>&1 | tee release_h2h.log
  ```

- [ ] Run the full anchored pool described in
  [`scripts/elo_pin/README.md`](../scripts/elo_pin/README.md) at both:
  - STC `10+0.1`
  - LTC `90+0.5`
- [ ] Use exactly 10 uniquely named workers for each pin.
- [ ] Match the fixed pool, opponent binaries, anchors, opening book, engine
  settings, adjudication, Ordo configuration, and games per pairing used by
  the comparison release.

  Start the STC master. This example expects 10 workers, giving 800 games per
  pairing at 80 games per worker.

  ```sh
  cd ~/elo_pin
  PIN_ROOT="$PWD" \
  NAME_ID="SHAYVERI v2.7.0 / NNUE" \
  NET= \
  TC=stc \
  GAMES_PER_PAIR_PER_WORKER=80 \
  REGISTER_SECONDS=300 \
  ./master.sh
  ```

  Start the LTC master separately. This example expects 10 workers, giving 200
  games per pairing at 20 games per worker.

  ```sh
  cd ~/elo_pin
  PIN_ROOT="$PWD" \
  NAME_ID="SHAYVERI v2.7.0 / NNUE" \
  NET= \
  TC=ltc \
  GAMES_PER_PAIR_PER_WORKER=20 \
  REGISTER_SECONDS=300 \
  ./master.sh
  ```

  Start one worker process on each participating machine during the
  registration window.

  ```sh
  cd ~/elo_pin
  PIN_ROOT="$PWD" ./worker.sh
  ```

  Monitor the active pin from another shell.

  ```sh
  cd ~/elo_pin
  PIN_ROOT="$PWD" ./watch.sh
  ```

- [ ] Review the results for crashes, illegal moves, stalls, and time forfeits.

  ```sh
  cd ~/elo_pin
  RUN_ID="$(cat current_run_id)"
  grep -REni \
    'illegal|crash|disconnect|forfeit on time|lost on time|timeout' \
    "results/$RUN_ID/logs" || true
  ```

- [ ] Preserve the run configuration, combined PGN, and complete Ordo output.

  ```sh
  cd ~/elo_pin
  RUN_ID="$(cat current_run_id)"
  cat "results/$RUN_ID/results.txt"
  cat "results/$RUN_ID/h2h.txt"
  ```

## 4. Finalize documentation and release metadata

- [ ] Ensure [`CHANGELOG.md`](CHANGELOG.md) describes every user-visible and
  engine-relevant change in the release.
- [ ] Remove `(Unreleased)`, candidate ranges, and pending-validation wording.
- [ ] Confirm the changelog heading contains the final version and title.
- [ ] Confirm the exact default NNUE and whether it is embedded or external.
- [ ] Update [`README.md`](../README.md), [`ELO.md`](ELO.md), and
  [`index.html`](../index.html) with the new release and applicable results.
- [ ] Confirm all published numbers match the preserved release evidence.
- [ ] Remove obsolete internal names, paths, stale versions, and private
  milestone wording from public documentation.

If the default NNUE changed:

- [ ] Update the [`Makefile`](../Makefile) and engine integration.
- [ ] Update the network cases in
  [`build-release.yml`](../.github/workflows/build-release.yml).
- [ ] Update the mapping in
  [`write-release-readme.sh`](../.github/scripts/write-release-readme.sh).
- [ ] Confirm the network exists under its exact advertised filename.
- [ ] Confirm the packaged `README.txt` describes it correctly.

## 5. Finalize the release commit

- [ ] Review the complete change from the previous public tag.
- [ ] Confirm the finalized commit is the commit that passed the release gates.
- [ ] Confirm the working tree is clean.
- [ ] Push the finalized release branch.

## 6. Merge and tag

- [ ] Merge the finalized release commit into `main`.
- [ ] Confirm local `main` matches `origin/main`.
- [ ] Create an annotated tag from the finalized commit.

  ```sh
  git tag -a v2.7.0 -m "SHAYVERI v2.7.0"
  git show --stat v2.7.0
  ```

- [ ] Push `main` and the tag.

  ```sh
  git push origin main
  git push origin v2.7.0
  ```

## 7. Validate release packages

The release workflow checks out the requested tag, so the tag must be pushed
before this step.

- [ ] Run the workflow from `main` without publishing.

  ```sh
  gh workflow run build-release.yml \
    --repo shahyaranfaz/SHAYVERI \
    --ref main \
    -f tag=v2.7.0 \
    -f publish=false
  ```

- [ ] Confirm every platform job succeeds.
- [ ] Download and inspect every archive and checksum.
- [ ] Start each packaged binary and verify `uci`, `isready`, the production
  option list, the default NNUE, and a legal `bestmove`.

## 8. Publish and verify

- [ ] Run the validated workflow with publishing enabled.

  ```sh
  gh workflow run build-release.yml \
    --repo shahyaranfaz/SHAYVERI \
    --ref main \
    -f tag=v2.7.0 \
    -f publish=true
  ```

- [ ] Confirm the release uses the correct annotated tag, title, and changelog
  notes.
- [ ] Verify the published checksums and source archives.
- [ ] Test at least one downloaded package outside the source tree.
- [ ] Confirm the release page, [`README.md`](../README.md),
  [`ELO.md`](ELO.md), and [`index.html`](../index.html) identify the new release
  as current.
