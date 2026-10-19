# Benchmark harness

`benchmark.c` measures toolkit internals directly. It compiles the toolkit into the benchmark executable; the demo and editor use the shared library. Build the harness with `./build.sh core` or `./build.sh`.

## Arguments

```text
mkguibench <layout|render|dirty|frame|present> <fill|buttons|text|mixed> <items> <width> <height> <iterations>
```

| Mode | Timed operation |
|---|---|
| `layout` | Recompute the layout of the existing widget tree |
| `render` | Clear and render the full framebuffer, including queued text; layout is already computed |
| `dirty` | Mark the first workload item dirty (the window for `fill`) and render its region, including queued text |
| `frame` | Mark the whole window dirty, lay out, acquire a writable buffer, render, and submit presentation |
| `present` | Submit the existing framebuffer without laying out or rendering widgets |

| Workload | Contents |
|---|---|
| `fill` | One window and no child widgets; `items` is ignored |
| `buttons` | Buttons in rows of up to eight |
| `text` | Labels in rows of up to eight |
| `mixed` | Repeating button, label, checkbox, progress, input, and slider widgets in rows of up to eight |

For non-fill workloads, the total widget count is `2 + ceil(items / 8) + items`: window, VBox, row HBoxes, and workload items. Such workloads require at least one item and must fit the toolkit's widget limit. Width and height arguments accept 64 through 16384; iterations accept 1 through `UINT32_MAX`.

The output's `width` and `height` are the actual client dimensions used by the renderer. Layout, scaling, and content minimums can affect them; compare these fields when comparing runs. The workload contains no icons.

## Running

Run from the repository root with a working display server. The development host uses CPU 9 and FIFO priority 99:

```sh
taskset -c 9 chrt -f 99 out/linux/mkguibench layout mixed 256 1920 1080 1000
taskset -c 9 chrt -f 99 out/linux/mkguibench render mixed 256 1920 1080 1000
taskset -c 9 chrt -f 99 out/linux/mkguibench dirty mixed 256 1920 1080 1000
taskset -c 9 chrt -f 99 out/linux/mkguibench frame mixed 256 1920 1080 1000
taskset -c 9 chrt -f 99 out/linux/mkguibench present fill 0 1920 1080 1000
```

Choose an available CPU and obtain permission to use the requested scheduling policy on your system. `MKWIN_SCALE=1` fixes the toolkit scale when comparing machines with different display settings. Windows builds provide `out/windows/mkguibench.exe` with the same arguments.

## Timing boundaries

Context/window creation, initial layout/rendering, and an initial synchronization occur before timing. The selected mode then runs 32 warm-up iterations. Presentation/frame warm-up work is synchronized before the measured batch.

One timer covers all requested iterations. In `frame` and `present`, a final `mkwin_sync()` is included in that timer. On X11 this waits for the server to process the submitted requests; it does not measure scanout or guarantee that a compositor has displayed the frame. Framebuffer checksumming happens after timing.

On Linux, `frame` alternates two XShm images. Buffer acquisition waits for outstanding work on the image being reused. Rendering can also repair damage missed by that image, while presentation submits the current frame's damage. `render` repeatedly draws into its already acquired framebuffer, without buffer acquisition or presentation. These are different execution paths and working sets.

## Output

The harness prints one row of `key=value` fields:

| Field | Meaning |
|---|---|
| `mode`, `workload`, `items`, `widgets`, `width`, `height`, `iterations` | Requested operation and actual workload dimensions/counts |
| `total_ns` | Elapsed nanoseconds for the batch, including the final synchronization where applicable |
| `ns_iteration` | `total_ns / iterations` |
| `unit` | `widget` for layout, `dirty_pixel` for dirty rendering, otherwise `pixel` |
| `ns_unit` | Time per iteration divided by the normalization-unit count |
| `minimum_gbps` | `4 * units / ns_iteration`, in decimal GB/s; zero for layout |
| `buffer_acquire_us` | Mean buffer-acquisition/damage-repair setup time, in frame mode |
| `layout_us` | Mean layout time, including OpenGL-child synchronization, in frame mode |
| `render_us` | Mean drawing time, including text, render callback, notifications, and tooltip, in frame mode |
| `blit_submit_us` | Mean presentation submission time in frame mode |
| `drain_us` | Final synchronization time divided by iterations, in frame/present modes |
| `checksum` | Hash sampled from every 64th framebuffer pixel after timing |

The four frame-component fields are zero outside `frame` mode because those modes do not collect the component timers. In particular, `render_us=0.000` in `render` mode is expected; `ns_iteration` is its rendering measurement.

Frame component sums need not equal `ns_iteration / 1000`: the outer timer also includes loop/instrumentation overhead, dirty marking, popup handling, and the final synchronization. `blit_submit_us` is submission time; buffer reuse and the final drain account for waits elsewhere.

`minimum_gbps` counts four bytes per normalized pixel. It is a framebuffer-throughput normalization, not measured memory-bus traffic. Drawing also reads and rewrites pixels, reads glyphs, and traverses widget data. The dirty-pixel count is the first item's rectangle, rather than a hardware traffic counter.

No current performance baseline is published here. Comparisons should record the source revision, compiler flags, CPU, display path, scale, actual output dimensions, and command. Frame and presentation measurements need the target display server; a virtual X server does not characterize desktop presentation cost.
