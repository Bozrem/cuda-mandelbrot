# CUDA Mandelbrot — Stage Planning

**End goal:** Produce 4K@60 HDR video with effectively infinite Mandelbrot zoom.

Work is split into two majors:

1. **Major I — Host pipeline:** Get the *look*, motion, and deep zoom *correct* (slow is fine).
2. **Major II — Device pipeline:** Keep frames on the GPU and hit production rate/quality.

Major I follows a simple Option-A style app. Major II is a deliberate refactor toward an Option-D style device-resident pipeline—not an incremental tweak of `memcpy`-every-frame.

---

## Design tips (make the I → II jump cheaper)

These are cheap in Major I and expensive to retrofit later:

- **Pure math, no I/O.** Escape, smooth iteration, palette/time mapping, and transfer functions should be plain functions (eventually `__host__ __device__` headers). No `fstream`, FFmpeg, or CUDA runtime inside them.
- **`FrameParams` early.** Center, zoom, time, max-iter, resolution, palette phase—one struct per frame. Major II just uploads it; it shouldn’t invent a new control model.
- **Sinks are adapters.** PGM/PNG and FFmpeg are outputs, not the program’s spine. Same for a future NVENC sink.
- **Prefer a useful working type over `uint8`.** Raw iteration or smooth float on the GPU; 8-bit PGM is a *view* of that data, not the canonical image.
- **Tag where memory lives** when you introduce buffers (`Host` vs `Device`). Even a comment/enum beats silently assuming host pointers.
- **Golden frames.** Before Major II, freeze a few shallow and deep stills + their param files. Port = match those, then go faster.
- **Reference orbits can stay on the host.** Perturbation’s high-precision reference orbit is a natural CPU job even in Major II; the delta field is what must stay device-resident.

---

# Major I — Host-Oriented Pipeline

## Architecture

**Pattern:** Simple monolithic / lightly split app (Option A). GPU does escape (and later perturbation deltas); everything else runs on the host.

```text
FrameParams → [GPU: escape / delta field] → D→H → [CPU: color / tonemap]
                                              → stills (PGM/PNG/EXR)
                                              → FFmpeg (slow video OK)
```

**What lives where**

| Piece | Side | Notes |
|-------|------|--------|
| Escape / delta rendering | Device | Hot compute only |
| High-precision reference orbit | Host | Perturbation; MPFR / floatexp / similar |
| Color, palette animation | Host | Research-friendly; easy to iterate |
| Encode | Host (FFmpeg) | Accept PCIe + encode cost |
| App structure | Few `.cu` + host `.cpp`, thin launch wrappers | Prefer clarity over modules |

**Intentional non-goals:** 4K60, NVENC, CUDA Graphs, zero-copy pipelines. Prove shots and math first.

**Precision ladder (deep zoom):** `float` → `double` (still shallow) → **perturbation** (HP reference + low-precision deltas) → series approximation / better glitch handling as depth demands.

## Substages

| ID | Name | Done when |
|----|------|-----------|
| **I.0** | Toolchain | Hello-CUDA (e.g. vector add) runs on the test machine. |
| **I.1** | Naive escape + still | Kernel fills per-pixel iteration; host writes a 4K PGM; known points match expectations. |
| **I.2** | Smooth value | Continuous/smooth escape (not raw iter alone); stills look less banded. |
| **I.3** | Host color | Palette research on CPU; time-varying color that doesn’t strobe; scrubbing `t` looks intentional. |
| **I.4** | Shot + host video | `FrameParams` + keyframe zoom path; FFmpeg produces a clip (any res/fps). Slow is OK. |
| **I.5** | Perturbation (infinite zoom) | HP reference orbit on host; GPU renders delta field; deep zooms past `float`/`double` limits; basic glitch detect/re-ref. |
| **I.6** | Deep-zoom stamina | Series approximation (or equivalent skip) + precision strategy good enough for long zoom shots without melting the clock. |
| **I.7** | HDR *look* on host | Working color defined (e.g. linear RGB); PQ/HLG or EXR stills and/or tonemapped SDR preview; not necessarily realtime. |
| **I.8** | Freeze references | Checked-in goldens: shallow + deep frames, params, and short reference clips for Major II diffs. |

---

# Major II — Device-Resident Pipeline

## Architecture

**Pattern:** Device frame graph / ring buffer (Option D). Host submits per-frame params (and reference-orbit data as needed); pixels stay on the GPU through encode.

```text
[params (+ ref orbit)] → Escape/Δ → Color → Tonemap/PQ → NVENC → bitstream
                         ↑______ device buffer pool / streams ______↑
```

**What lives where**

| Piece | Side | Notes |
|-------|------|--------|
| Buffer pool, streams, sync | Device runtime | Double/triple-buffer for overlap |
| Escape / perturbation field | Device | Same math as Major I, new residency |
| Color + transfer | Device | Shared headers with Major I math |
| Reference orbit | Host (typical) | Upload tables/coefficients per frame or shot |
| Encode | Device (NVENC) | No full-frame D→H on the hot path |
| Preview/debug | Rare D→H | Downscale or single-frame dump only |

**App shape:** Explicit stages + runtime (closer to Option B modules under an Option D dataflow). PGM/FFmpeg remain debug sinks, not the default.

**Success bar:** Same look as Major I goldens; 4K@60 HDR without PCIe as the bottleneck.

## Substages

| ID | Name | Done when |
|----|------|-----------|
| **II.0** | Runtime skeleton | Device buffer pool + stream(s); N frames with **no** hot-path D→H. |
| **II.1** | Port compute | Escape / perturbation delta path on device; matches I.8 goldens within tolerance. |
| **II.2** | Port color | Color + transfer on GPU via shared math; goldens still match. |
| **II.3** | Device encode | NVENC (or equivalent) outputs HDR bitstream; preview path optional and explicit. |
| **II.4** | Rate target | Compute-encode overlap; hold 4K60 on target hardware. |
| **II.5** | Polish / graph | CUDA Graphs if topology is stable; optional temporal reuse or further SA—only with measured wins. |

---

## Backlog

Ideas to revisit
- **Temporal frame reuse** under slow zoom (lossy); prototype on host if curious, implement for real only in II.5 if metrics say so.
- Multi-GPU, adaptive iteration caps, better glitch repair, UI scrubber, etc.
