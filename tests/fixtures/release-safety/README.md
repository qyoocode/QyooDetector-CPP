# Accepted-wrong release safety regression

`c-max64.png` is a 64×64 PNG produced by the public C generator audit working
source with ID 68,719,476,735. The manifest records the exact command, generator
C-source hashes, image digest and expected payload. It is a generated symbol,
not a photograph. No private renderer source is required to run the check.

The inspected detector accepts this exact file as **0**. This is an
accepted-wrong result and a release blocker. `make test-release-safety` must
fail until the detector either returns the intended payload or rejects the
image. This audit records the defect without changing qualified decoding.
