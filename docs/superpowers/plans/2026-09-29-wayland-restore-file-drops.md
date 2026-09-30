# Native Wayland restoration and external audio-file drops

The user reports that moving/resizing or restoring FL Studio sometimes leaves
menus unresponsive, that taskbar activation can show only a small menu strip,
and that dropping audio from Dolphin/desktop into FL Studio does nothing.
Publish the tested changes to Audion main. Preserve existing plugin input,
Nexus rendering, ntsync and audio configuration.

## Evidence and design

- Installed Wine includes the preceding pointer-confinement patch.
- The live input observer delivered clicks to FL's menu and popup windows; the
  user reported that menus worked during this attempt. No unsupported custom
  resize change is inferred from that successful run.
- The trace records FL's minimized window at (-128,-16), size160x31, retaining
  WS_MINIMIZE. Existing Wayland restoration requires a -32000 sentinel instead.
  An isolated KWin test reproduces failed restoration with that iconic geometry.
- Track received compositor activation independently of acknowledged geometry.
  Latch minimized reactivation across coalesced configurations, and consume it
  on the application thread to invoke the existing SC_RESTORE path.
- Incoming Wayland data-device enter/motion/drop callbacks are empty. Import
  local text/uri-list offers through Wine's existing OLE/WM_DROPFILES bridge.
  Keep transfer IO bounded and off the event/application thread; use a generation
  checked message to deliver the completed payload to the target window thread.
  Support incoming file copies; preserve the clipboard backend. Outgoing drag
  and drop, remote downloads and OLE hover previews are outside this change.

## Verification sequence

1. Failing lifecycle/import tests before implementation.
2. Compile PE and Unix driver components and verify URI payloads, transfer
   limits, cancellation, dispatch and restoration state transitions.
3. Isolated KWin restoration test plus real FL taskbar/menu/plugin tests and
   Dolphin-to-Sampler/Playlist file drops.
4. Existing coordinate, confinement and move regressions; patch application,
   repeat upgrades and preservation of staged user changes.
5. Full incremental build, independent review, final documentation and publish.

No audio callback, PipeASIO, scheduling or kernel setting is modified.
