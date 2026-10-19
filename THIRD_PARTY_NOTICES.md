# Third-party notices

Qyoo source is covered by the repository's BSD-3-Clause [LICENSE](LICENSE).
Third-party components retain their own licenses.

## Configurable Math Library (CML)

The 301 files in `cml/` match upstream CML commit
[`0bb27c8637dd9cdc92b2f376a9e8c08eb649920f`](https://github.com/demianmnave/CML/tree/0bb27c8637dd9cdc92b2f376a9e8c08eb649920f/cml)
byte for byte (Git blob SHA-1 comparison, 18 October 2026). `version.h`
identifies version 2.2.2. Upstream [README](https://github.com/demianmnave/CML/blob/0bb27c8637dd9cdc92b2f376a9e8c08eb649920f/README.md)
identifies the Boost Software License 1.0; the complete unmodified upstream
license is in [LICENSES/CML-Boost-1.0.txt](LICENSES/CML-Boost-1.0.txt).

The upstream headers themselves contain `@@COPYRIGHT@@` placeholders. They
are preserved verbatim, rather than replaced with an invented copyright
holder or relicensed as Qyoo code. This identifies a matching source snapshot;
it does not establish which upstream checkout was originally imported.

## libgd

The command-line and core builds link the system libgd; its headers and libraries are not vendored. The libgd 2.3.3 notice is reproduced in
[LICENSES/libgd.txt](LICENSES/libgd.txt). Include the applicable
libgd notice in user-accessible supporting documentation when distributing
software using it. Other linked image-codec libraries may require their own
notices; distributors must inspect their actual dependency configuration.

## Image fixtures

Generated fixtures and historical photographs are distinct from dependency
source code. Repository history does not supply a separate photographer or
redistribution grant for the camera photos. Before a public release, the owner
must confirm their rights to redistribute those photographs. Existing assets
are retained pending that review.


Security maintenance is separate from license compliance. See the audit's
scoped libgd advisory review; do not infer vulnerability or safety solely from
a version string.
