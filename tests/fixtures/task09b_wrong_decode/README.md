# Task 09B accepted-wrong fixtures

These seven JPEGs are byte-for-byte copies of the accepted-wrong inputs first
found in the Task 09 1,332-case synthetic safety matrix. They are repository
fixtures rather than generator outputs: later changes to the synthetic
generator cannot silently remove or alter them.

`manifest.json` records image hashes, generator/render provenance, expected and
observed 36-bit carriers, differing MSB-first bit indexes, and the complete
source transform. `reference-traces/` preserves the accepted candidate,
carrier/template fit, and all 36 deterministic sample measurements from the
Task 09 experimental binary.

The exact Task 09 multiscale experiment was preserved locally before this
fixture was added:

- branch: `task-09b-safety-hardening`
- commit: `6fc3d26000e72bc0296e3e61a01da956aef6f0db`
- tree: `5aa37d4e9986c1ecee3e521e6f950695987b3aaf`
- detector binary SHA-256:
  `46833c3ccc1c6377fb5e51c207369841acc40b37ffd7430f9ca39f9c9cde3d25`

Rebuild the metadata from the original local Task 09 evidence with:

```sh
python3 tests/fixtures/task09b_wrong_decode/freeze_from_task09.py
```

The copied JPEG hash check is mandatory and fails if any fixture byte changes.
