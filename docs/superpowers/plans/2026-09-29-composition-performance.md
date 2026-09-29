# Composition presentation performance

Approved scope: reduce Nexus accelerated GUI overhead without weakening immutable completed-frame snapshots, synchronization, Wayland input, alpha blending or resize handling. No audio scheduling or ntsync changes.

1. Measure baseline and add regression coverage for repeated snapshot acquisition, RGBA composition and repaint of unchanged content.
2. Cache immutable DXVK snapshots per successful present. Keep retained snapshots immutable; invalidate on present and resize under existing buffer/context lock order.
3. Reuse Wine's per-visual conversion target/context state; use Direct2D's existing GPU bitmap conversion instead of compiling shaders on every frame. Keep a read-only GDI DC for an unchanged serial, replaying its pixels when necessary without another GPU readback. Release with an empty dirty rectangle. Release resources on resize/content replacement/destruction. Preserve fallback for unversioned providers.
4. Pace from elapsed composition time rather than adding a full refresh interval after rendering. No busy waiting, high-priority threads or global timer-resolution changes.
5. Run pixel/lifetime/concurrency, D2D stress, repeat-build and Wayland checks; compare baseline and candidate process CPU time with the same paced rendering workload. Review and integrate the tested patch stack, document remaining one-readback-per-new-frame limitation and build/install/run commands.

A true GPU-only compositor requires correct alpha/child-window interoperation. This implementation eliminates repeated readback of unchanged frames and repeated conversion allocations without substituting an opaque HWND swapchain. It does not claim zero-copy presentation or complete DirectComposition.

## Follow-on after live feedback

The user reports routing-graph lag unchanged after the caching candidate. Continue
with a GPU-only presentation path for a single opaque Nexus visual, avoiding
readback completely. Restrict automatic eligibility to Nexus-owned JUCE windows;
other transparent or complex targets retain GDI. Test content replacement,
resize, lifecycle and native Wayland input. Verify live trace actually selects
the path before asking the user to compare. Do not publish a lag-fix claim from
synthetic throughput alone.
