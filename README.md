# Helios for macOS

[![tests](https://github.com/fiazsami/helios/actions/workflows/tests.yml/badge.svg)](https://github.com/fiazsami/helios/actions/workflows/tests.yml)

<img src="thumbnail.png" alt="Helios screen saver" width="240" align="right">

Helios is Terence M. Welsh's screen saver that simulates particles attracting and repelling one another. It comes from the Really Slick Screensavers collection and was ported to macOS by Nick Zitzmann. This fork adds a C++ test suite, continuous integration, a line-coverage gate and fixes for five defects. It also moves the project to macOS 26.

## What changed in this fork

The work was done between 21 and 23 August 2026: 66 commits, +5,925 / −393 lines, all through 18 pull requests.

| Area | Result |
| --- | --- |
| Tests | 198 test cases in a single-binary C++17 harness ([`tests/harness.h`](tests/harness.h)): self-registering `TEST()` cases, with no third-party framework |
| Coverage | Line coverage of the C++ core (`rsMath`, `Implicit`, `Rgbhsl`, `util`; 2,388 lines) rose from **5.4% to 96.5%**, measured with clang source-based coverage (`llvm-cov`) |
| Defects | Five fixed, each with a test that fails without the fix (see below) |
| Test quality | Fixes were checked by deliberately re-introducing the bug (mutation testing); for example, the twin fix in Hyperspace, [`d2e9cb4`](https://github.com/fiazsami/hyperspace/commit/d2e9cb4), records three mutants, all caught |
| Platform | Deployment target moved from macOS 10.9, which no longer links, to [macOS 26](https://github.com/fiazsami/helios/commit/c3c9bca). Signing is ad hoc, with no developer team in the project; CI enforces both |
| CI | [GitHub Actions](.github/workflows/tests.yml) builds and runs the test suite, builds Release and Debug with `xcodebuild`, and verifies the code signature |

Drawing code that calls OpenGL directly (`Helios.cpp`, `HeliosView.mm`) is outside the coverage scope.

## Defects found and fixed

| Defect | Effect | Fix |
| --- | --- | --- |
| Vertex-buffer names generated in the `impSurface` constructor | Crashed without an OpenGL context, so `impCubeVolume` could not be constructed in a test at all | [`c420260`](https://github.com/fiazsami/helios/commit/c420260) |
| `impCubeVolume::frame` never initialised | The surface cache could report a false hit on the first frame | [`b7b2fb7`](https://github.com/fiazsami/helios/commit/b7b2fb7) |
| The same frame counter wrapped at 65,536 | The same false hit returned after the wrap; review found the first fix was only half a fix | [`8d723e6`](https://github.com/fiazsami/helios/commit/8d723e6), [`ad4f708`](https://github.com/fiazsami/helios/commit/ad4f708) |
| Unsigned clamp of out-of-volume crawl points | Points were sent to the far face of the volume instead of the near one | [`82bb150`](https://github.com/fiazsami/helios/commit/82bb150) |
| `#ifdef` where every call site used `#if USE_TRIANGLE_STRIPS` | A function with no callers was compiled in | [`fd2a4ce`](https://github.com/fiazsami/helios/commit/fd2a4ce) |

The C++ core is shared with [Hyperspace](https://github.com/fiazsami/hyperspace). Each core fix landed in both repositories as a matched pair of pull requests.

## Build and test

You need macOS 26 and Xcode, or the Xcode command-line tools for the tests alone.

- **Tests:** compile every source under `rsMath`, `Implicit`, `Rgbhsl`, `util` and `tests` with `clang++ -std=c++17`, link with `-framework OpenGL`, and run the binary. The exact steps are in [`.github/workflows/tests.yml`](.github/workflows/tests.yml).
- **Screen saver:**

  ```sh
  xcodebuild -project Helios.xcodeproj -target Helios -configuration Release build
  ```

  Then double-click the built `Helios.saver` to install it.

## How the work was done

The tests, fixes and reviews were written by coding agents (Claude Code) running inside a harness, under the direction of [Fiaz Sami](https://github.com/fiazsami). The harness provides a coverage ratchet, guard hooks and an independent verifier. Every change reached `master` through a pull request with CI; review findings, and the fixes they prompted, are recorded in pull request comments and commit messages. The harness, the coverage data and dated write-ups of what went wrong are in [fiazsami/screen-savers](https://github.com/fiazsami/screen-savers).

## Credits and licence

- Helios © 2001–2010 Terence M. Welsh.
- macOS port by Nick Zitzmann ([nickzman/helios](https://github.com/nickzman/helios)).
- Licensed under the GPL-2.0 ([`LICENSE.txt`](LICENSE.txt)). The `rsMath` and `Implicit` libraries are LGPL-2.1 or later, per their file headers.
