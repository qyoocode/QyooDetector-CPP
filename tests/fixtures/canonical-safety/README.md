# Canonical safety fixtures

These 16 compact known-answer PNGs retain IDs 0, 1, 2 and 68719476735 at
512, 256, 128 and 64 pixels. Expected payloads come from the numeric input,
not detector output. `recorded_outcome` is historical evidence, not a changed
expectation: zero/one/two decode at every size; max decodes through128 and
rejects at64 on native core60aa253. No image accepts a wrong ID.

Native512 files are byte-identical actual current Web exports from
`Qyoo-Web/src/generator/qyoo-renderer.ts`, SHA256
`60843878a6909bcb4e7afd01470633f0d8d295bd71f476daec979b142a540ef0`.
The ID source SHA256 is
`fd9c93cc9ad43e24fd6f7bfd9480fd5a3e8b210cffb6942279e42a9e0f3797d8`.
Rendering: `renderQyooPng(decimal,512)`; opaque RGBA pixel-center analytic
sampling of the256-unit canvas, black circle center128/r102 plus lower-right
square, white set-bit disks centers65..190 step25/r10. Bits are36 row-major
MSB first. Reduced images use Pillow12.3.0 `Image.open(native512).resize(
(size,size), Image.Resampling.LANCZOS).save(path)`. They are reductions,
not native Web API exports;128/64 are outside its export API size range.
Image SHA256s in the manifest pin compressed bytes. Generation requires the
Web source/Node TypeScript stripping and Pillow; running tests needs neither.

Run from the repository root with a dedicated existing scratch location:

```sh
python3 tests/test_generated_safety.py --manifest tests/fixtures/canonical-safety/manifest.json --work-dir .codex-work/canonical-safety --report .codex-work/canonical-safety/report.json
```

The runner fails wrong accepted payloads and processing errors; rejections are
reported separately and permitted by this safety test. It is not a recall gate
or camera qualification. The qualification audit also retains52 pre-existing
Web vectors and54 new conditions; these fixtures intentionally remain small.
