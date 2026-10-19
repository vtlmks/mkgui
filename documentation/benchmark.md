# mkgui benchmark

`mkguibench` is a unity-build benchmark harness with deterministic layout, full-render, dirty-render, presentation, and complete-frame modes.

## Command line

```text
mkguibench <layout|render|dirty|frame|present> <fill|buttons|text|mixed> <items> <width> <height> <iterations>
```

The harness performs 32 warm-up iterations, times one uninterrupted iteration batch, synchronizes completed presentation where applicable, and computes a framebuffer checksum after timing.

The output contains:

| Field | Meaning |
|---|---|
| `ns_iteration` | Mean time for one requested operation |
| `unit` | Normalization unit: widget, pixel, or dirty pixel |
| `ns_unit` | Mean time per normalization unit |
| `minimum_gbps` | Four bytes per affected pixel divided by elapsed time; actual read and write traffic can be higher |
| `layout_us`, `render_us`, `blit_submit_us` | Mean internal components for complete-frame mode; zero in other modes |
| `buffer_acquire_us` | Mean time acquiring a reusable framebuffer and combining its damage history, in complete-frame mode |
| `drain_us` | Final presentation drain divided by iteration count |
| `checksum` | Sampled framebuffer checksum |

Layout, full render, and dirty render execute entirely in the client process after window initialization. Presentation and complete-frame results include the display server and must be measured on the target display path. Xvfb results for those modes are correctness references only.

Linux complete frames alternate two XShm images. Layout runs before buffer acquisition; acquisition waits only for outstanding completion events for the image about to be reused. Dirty redraws repair changes missed by that image, while presentation transfers only the current frame's damage. The final drain remains inside `total_ns`, so queued work is included in the result.

## Controlled Linux run

On the Ryzen 9 9950X3D development machine, run the workload on CPU 9 with FIFO priority 99:

```bash
taskset -c 9 chrt -f 99 out/linux/mkguibench render mixed 256 1920 1080 5000
```

Hardware counters run under the same affinity and policy:

```bash
taskset -c 11 chrt -f 99 perf stat -r 3 -e cycles,instructions,branches,branch-misses,l1-dcache-loads,l1-dcache-load-misses taskset -c 9 out/linux/mkguibench render mixed 256 1920 1080 5000
```

## Ryzen 9 7950X baseline

Recorded on a Ryzen 9 7950X with boost disabled, CPU 1 isolated, GCC `-O2 -march=x86-64-v2`, and a 2026-07-11 build. Rendering measurements used Xvfb only for window initialization; timed render modes did not present.

| Mode | Workload | Size | Widgets | Time | Normalized |
|---|---|---:|---:|---:|---:|
| layout | mixed 32 | 1920x1080 | 38 | 1.311 us | 34.50 ns/widget |
| layout | mixed 256 | 1920x1080 | 290 | 10.562 us | 36.42 ns/widget |
| layout | mixed 2048 | 1920x1080 | 2,306 | 85.222 us | 36.96 ns/widget |
| layout | mixed 16384 | 1920x1080 | 18,434 | 722.602 us | 39.20 ns/widget |
| render | fill | 1280x720 | 1 | 30.914 us | 119.25 GB/s minimum |
| render | fill | 1920x1080 | 1 | 70.952 us | 116.90 GB/s minimum |
| render | fill | 3840x2160 | 1 | 541.048 us | 61.32 GB/s minimum |
| render | buttons 256 | 1920x1080 | 290 | 438.074 us | 18.93 GB/s minimum |
| render | text 256 | 1920x1080 | 290 | 205.986 us | 40.27 GB/s minimum |
| render | mixed 256 | 1920x1080 | 290 | 297.025 us | 27.93 GB/s minimum |
| dirty | mixed 256 | 1920x1080 | 290 | 2.748 us | 0.382 ns/dirty pixel |

The 2,306-widget layout workload retires 4.25 instructions/cycle with 0.080% branch misses and uses 72.5% of the Zen 4 six-slot dispatch width. Empty dispatch slots attribute 15.8% of capacity to backend stalls and 11.6% to frontend starvation. Its cost remains close to linear through 18,434 widgets, rising 13.6% per widget from the smallest case.

The mixed render workload retires 4.58 instructions/cycle with 0.071% branch misses and uses 71.0% of dispatch width. Backend stalls account for 23.6% of dispatch capacity and frontend starvation for 5.5%, so branch prediction and frontend delivery are not the current limit. The measured limiter is backend availability dominated by framebuffer and glyph cache traffic plus integer blend work.

The 1080p fill establishes a cached working-set store rate of 116.9 GB/s. At 4K the 33.2 MB framebuffer reaches the CCD cache-capacity boundary and the rate falls to 61.3 GB/s. The mixed workload's four-byte minimum traffic is 23.9% of the 1080p fill rate, but this is not a bandwidth roofline fraction because rounded rectangles, glyphs, and controls also read and rewrite destination pixels.

Presentation under Xvfb measured 176.2 us for a completed 1080p XShm transfer. A complete mixed frame measured 543.3 us: 20.5 us layout, 320.5 us rendering, 2.2 us presentation submission, and approximately 200 us waiting for completed XShm work. Neither number characterizes a real X server, compositor, GPU, or scanout path.
