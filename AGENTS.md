# Working rules

- Read README.md and docs/HANDOFF.md before changing architecture.
- Preserve the Eden-NX2, Windows builder, game dumps and saves. They are references, not working trees of this project.
- Single-file NUP and directory .nup/00,01,... have equal priority. Use Input for all file access; never concatenate split files into a giant temporary file or read the entire game into RAM.
- No copyrighted game data, dumps, keys, saves, SD logs or local paths in commits/CI. Fixtures must be synthetic. Public SDK tools are the only external build payload.
- A successful probe is NOT a successful game launch. Report host tests, binary verification and console validation separately.
- Do not run raw guest code from writable heap memory or use unavailable kernel code-mapping SVCs. Study kaiHEN ldr:ro and upstream whole-module constraints first.
- Keep ldr:ro limits, SVC 0x36 handling, module order, TLS and runtime lifecycle explicit in the loader design.
- Preserve upstream notices and pin reviewed revisions. Upstream updates are manual reviews, never automatic replacements of runtime code.
- Run synthetic host tests and the strict Switch 2 NRO/link-map verifier for relevant changes. The probe does not use JIT, so a JIT marker is not required yet.
- Keep documentation of decisions, observed results and remaining limitations up to date. Never claim audio or saves work without a console test.
