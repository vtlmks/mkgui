# Security policy

mkgui is a small GUI toolkit maintained by a single developer. There is no formal release cadence and no backport policy - the supported version is "latest `master`".

## Reporting a vulnerability

Please do not open a public GitHub issue for security-sensitive reports (e.g. a crafted font, SVG, image, clipboard payload, or drag-and-drop payload that causes memory corruption, out-of-bounds read/write, or arbitrary code execution).

Instead, email **peter.fors@mindkiller.com** with:

- A description of the issue and its impact.
- A minimal reproducer (source file or small repository).
- The platform and compiler you observed it on.
- Any suggested fix, if you have one.

You will get an acknowledgement within a reasonable time. Once a fix lands on `master`, the issue and credit (if desired) can be disclosed publicly.

## Scope

In scope: memory safety bugs, undefined behaviour, and logic bugs in mkgui itself that can be triggered by untrusted input an application might plausibly feed to the toolkit.

Out of scope: bugs in bundled third-party code (PlutoSVG, PlutoVG, incbin.h) - please report those upstream. Denial-of-service by simply allocating enough widgets to exhaust memory is also out of scope.
