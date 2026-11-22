# Implementation handoff

## Current verified state

Milestone 0 is a deliberately small scaffold. It configures as a C++23 CMake project with Qt 6 Widgets, builds a desktop executable and an asset-free CTest executable, and retains arbitrary input bytes in `WorldFile`. The GUI can choose a `.blp` or `.xch` file and report whether its byte count equals the canonical v1.3 size. It does not yet identify, decode, edit, render or save a world. Minimap, statistics, validation and PNG export exist only in `plan.md`; campaign and installation workflows are explicitly deferred beyond 1.0.

The initial scaffold was configured and built in Release mode and its CTest test passed. Re-run the commands in `README.md` on the current machine before relying on that result.

## Next task: bounded world probe

Implement the first part of milestone 1 without adding write support.

1. Replace v1.3-only layout constants with immutable descriptors for revisions 1.0, 1.1, 1.2 and 1.3. Include header size, layer presence/count, object count/record width, expected payload size and the relevant revision rule.
2. Add small overflow-safe, little-endian readers for signed and unsigned 16-bit and 32-bit values over `std::span<const std::byte>`. A failed read must return a typed error with the first unavailable offset; it must never read, resize or modify the input.
3. Add a `WorldProbe` result that reports candidate revision, expected size, available complete sections, truncation or surplus length, and why the input is or is not safe for structured inspection. File extension alone is not evidence that a BLP is a world, and equal size alone is not sufficient proof.
4. Build synthetic byte arrays independently in tests. Cover all four expected sizes, every section boundary, unknown/later revisions, unrelated and matching-size arbitrary data, truncation at each scalar width, surplus bytes and values that exercise signed little-endian decoding.
5. Change the GUI inspector to show the read-only probe result and its uncertainty. Do not enable editing or saving.

Acceptance evidence:

- The original byte span is identical before and after every probe.
- Offset calculations are checked for overflow and out-of-range reads return diagnostics rather than exceptions or undefined behavior.
- Tests do not call the production serializer to construct their expected fixtures.
- A matching-size arbitrary BLP is not presented as a verified world.
- Configure, Release build and CTest succeed with no game installation or game assets.

## Explicit non-goals for this slice

Do not add header field editing, serialization, canvas rendering, game-asset loading, installation discovery, campaign support, ZIP handling or procedural generation. The following slice should decode a read-only header view with exact field offsets while retaining all original bytes. Revisions 1.0 and 1.1 remain read-only throughout the 1.0 product plan; only 1.2 and 1.3 will later gain structured writes.
