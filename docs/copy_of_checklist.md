## 5. Merge and tag

- [ ] Review the complete diff from the previous public tag.
- [ ] Rebuild and repeat affected gates after any engine or build-input change.
- [ ] Commit the finalized release, merge it into `main`, and confirm the
  working tree is clean.

```sh
git tag -a v2.10.0 -m "SHAYVERI v2.10.0"
git push origin main
git push origin v2.10.0
```

## 6. Build and publish

The tag must be pushed before running the release workflow.

```sh
gh workflow run build-release.yml \
  --repo shahyaranfaz/SHAYVERI \
  --ref main \
  -f tag=v2.10.0 \
  -f publish=true
```

- [ ] Confirm every build and publish job succeeds.

## 7. Verify the release

- [ ] Inspect the release title, notes, and assets.
- [ ] Test at least one downloaded package outside the source tree.
