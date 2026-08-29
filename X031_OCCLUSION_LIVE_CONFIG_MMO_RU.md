# ArenaMP X031 — Occlusion Stage2 / Live RAW Config / MMO Defaults

## Renderer / occlusion
- Terrain occluder no longer assembles one large rebased mesh every region update.
- Cached LAND cells are rasterized directly from their own position/index buffers.
- Side-plane frustum culling rejects terrain occluder cells behind/outside the camera before software rasterization.
- New setting: `[Camera] occlusion terrain frustum cull = true`.
- Profiler adds `Occl Terrain Rast` (terrain cells actually submitted to the rasterizer).
- X029 atomic profiler snapshots are preserved; no direct cross-thread reads of mutable cache counters.
- One reusable scratch LAND vertex array reduces repeated allocations during cell decode.
- Small-object culling performs the named `skipOcclusion` lookup only after software occlusion rejected the AABB.

## RAW server config hot reload
- `server/scripts/config.lua` is watched while the server is running.
- Default poll interval: `config.rawConfigReloadInterval = 2` seconds, clamped to 1..60.
- Reload is transactional: syntax/runtime/incomplete-save errors keep the previous working config active.
- On successful reload, live server rules are reapplied and connected players receive refreshed authoritative settings.
- Settings consumed directly from the global `config` table take effect on their next normal use/event.
- Startup-only structural settings (plugin paths/load order/process topology) still require a server restart.

## questIndex.json on restart
- `config.questIndexRefreshOnServerStart = true` by default.
- Every server process start marks the stored `custom/questIndex.json` as refresh-pending immediately and updates its restart metadata.
- Old entries are retained only for diagnostics; quest phasing remains untrusted/fail-closed until a fresh verified client upload arrives.
- The first valid fresh upload overwrites `questIndex.json`, clears the pending flag and re-enables trusted phasing.

## MMO defaults in Launcher
- ArenaMP graphics preset level 2 is now labelled `MMO (default)` and is the default for a fresh launcher/wizard instead of Auto.
- MMO graphics profile keeps 8192 view distance, conservative paging/occlusion cost and the X030 shadow-distance link.
- OSG Patch page adds `MMO (default)` and fresh defaults are aligned with it: `DrawThreadPerContext`, VBO on, display lists off, terrain/static occlusion on, force shaders on, forced per-pixel lighting off, one preload worker, one async physics worker, deferred AABB updates on.
- Existing explicit user launcher choices are preserved; only fresh/default state changes automatically.

## Removed legacy ArenaMP options
- Removed `--vanilla-build-server` and the vanilla TES3MP protocol/commit impersonation path. ArenaMP always advertises its ArenaMP protocol + compatibility hash.
- Removed hardcoded `mp.tes3mp.com` fallback; local fallback is `localhost` and the shipped client default also points to localhost.
- Removed `--hide-chat-history`.
- Chat can no longer enter Auto-hide or Hidden mode. F2 cycles only Visible / 30% / 60% opacity. Old `autohide` or `hidden` settings migrate to a visible 30% mode.
- Chat history is always enabled. Temporary UI hiding used internally by menu transitions remains intact.

## Validation performed
- X031 occlusion logic harness: cold radius-8 build 13 frames at budget 24, one-cell step 17 new cells, unlimited mode 289 cells.
- Side-frustum tests pass for both ordinary and reversed depth projections.
- Qt `.ui` XML parsed successfully.
- Lua syntax checks passed for `config.lua`, `configHotReload.lua`, `questIndexStore.lua`, `serverCore.lua`.
- Mock RAW reload passed: valid reload, syntax rollback, incomplete-save rollback, recovery reload.
- Mock quest-index startup refresh passed and updates restart metadata while keeping the index untrusted until fresh upload.

Full engine compilation was not run in this environment.
