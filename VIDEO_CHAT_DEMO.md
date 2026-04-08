# Video Chat Demo

A real-time 1-on-1 video chat application built on the Resync framework's `reason-realtime` middleware/adapter architecture. No database — all state is in-memory on the server.

## Quick Start

```bash
# From repo root
dune build @app --root demos/video-chat    # build client UI
dune build demos/video-chat/server/src/server.exe  # build server

# Set environment (or use .envrc with direnv)
export VIDEO_CHAT_DOC_ROOT=./_build/default/demos/video-chat/ui/src/
export VIDEO_CHAT_SERVER_INTERFACE=127.0.0.1
export VIDEO_CHAT_SERVER_PORT=8897

# Run the server
./_build/default/demos/video-chat/server/src/server.exe
```

Open `http://127.0.0.1:8897/` in a browser. Create a room, then open a second tab and join with the room ID.

## Architecture Overview

```
Browser Tab A                    Server (native OCaml)              Browser Tab B
┌─────────────┐                 ┌──────────────────────┐          ┌─────────────┐
│ React UI    │                 │                      │          │ React UI    │
│  ├ RoomPage │                 │  Dream HTTP Server   │          │  ├ RoomPage │
│  └ Store    │◀──WebSocket──▶ │  ├ Middleware.route   │◀──WS──▶ │  └ Store    │
│             │   /_events      │  ├ VideoChat_adapter  │          │             │
│ MediaCapture│                 │  └ handle_mutation   │          │ MediaCapture│
│ MediaTransport│               │                      │          │ MediaTransport│
└─────────────┘                 └──────────────────────┘          └─────────────┘
```

The demo uses the same `Middleware`/`Adapter` pattern as the ecommerce demo, but with an **in-memory adapter** instead of PostgreSQL + pg_notify.

## Data Flow

### Connection Lifecycle

1. Browser loads the SPA, `VideoChatStore.hydrateStore()` initializes a synced store
2. Store's `Synced.Define` runtime opens a WebSocket to `/_events`
3. Client sends `{"type":"select","subscription":"<client_id>"}` 
4. Server's `Middleware.route` receives it, calls `resolve_subscription` → returns `client_id` as the channel
5. `subscribe_websocket` registers the websocket under that channel, calls `Adapter.subscribe` to store a broadcast handler
6. `load_snapshot` returns initial state JSON with the channel as `client_id`
7. Client receives the snapshot, hydrates its state

### Joining a Room

```
Client A (existing)          Server                       Client B (joining)
                             │
                             │◀─ select (client_id_B)
                             │ ─ subscribe handler ──────▶ stores handler[B]
                             │ ─ snapshot ───────────────▶ Client B hydrates
                             │
                             │◀─ mutation join_room       (room_id, peer_id=B)
                             │
                             │  add_peer(B)
                             │  get_peers → [A, B]
                             │  send_to_peer(B, update_peers {peers: [A]})
                             │                             ── patch ──────────▶ SyncPeers
                             │
                             │  broadcast_to_room(~except=B, peer_joined {id: B})
  ── patch ─────────────────▶│
  PeerJoined                 │
```

Key details:
- Each peer subscribes to their own `client_id` as the channel — this enables direct `send_to_peer`
- `handle_mutation` processes `join_room`: sends `update_peers` back to the joiner with existing peers, then broadcasts `peer_joined` to everyone else
- `update_peers` only lists *other* peers (the joiner sees who's already there)
- The `PeerJoined` patch on Client A triggers the reducer to add the new peer to its room state

### Leaving a Room

When a client disconnects (navigates away or clicks Leave):
1. Client dispatches `LeaveRoom` action → server `handle_mutation` removes peer, broadcasts `peer_left`
2. WebSocket closes → `Middleware` calls `Adapter.unsubscribe` → adapter removes peer from room, broadcasts `peer_left` to remaining peers

### Store Actions and Patches

Actions come in two flavors:

| Action | Sent by | Mechanism |
|--------|---------|-----------|
| `JoinRoom` | Local client | Store mutation → server `handle_mutation` |
| `LeaveRoom` | Local client | Store mutation → server `handle_mutation` |
| `ToggleVideo` / `ToggleAudio` | Local client | Store mutation → server `handle_mutation` |
| `SyncPeers` | Server → joining peer | Patch (from `update_peers`) |
| `PeerJoined` | Server → existing peers | Patch (from `peer_joined` broadcast) |
| `PeerLeft` | Server → remaining peers | Patch (from `peer_left` broadcast) |
| `RemoteVideoFrame` | Server → other peers | Patch (re-broadcast with `peer_id`) |
| `RemoteAudioChunk` | Server → other peers | Patch (re-broadcast with `peer_id`) |
| `RemoteToggleVideo` / `RemoteToggleAudio` | Server → other peers | Patch (re-broadcast with `peer_id`) |

**Mutation flow**: Client dispatches action → `action_to_json` serializes it → sent as `{"type":"mutation","actionId":"...","action":{...}}` → server `handle_mutation` processes it → sends `ack` back to client, broadcasts result to other peers.

**Patch flow**: Server sends `{"type":"patch","payload":{...},"timestamp":...}` → client's strategy `decodePatch` → `updateOfPatch` (reducer) → state update.

## File Guide

### Server (OCaml)

| File | Purpose |
|------|---------|
| `server/src/server.ml` | Dream server: routes, `load_snapshot`, `handle_mutation`, static file serving |
| `server/src/VideoChat_adapter.ml` | In-memory adapter: rooms table, per-peer handler storage, `broadcast_to_room`, `send_to_peer` |

### Client (Reason)

| File | Purpose |
|------|---------|
| `ui/src/VideoChatStore.re` | Synced store: actions, reducer, serialization, custom strategy with `Synced.Define` |
| `ui/src/RoomPage.re` | Room UI: peer count, local/remote video, join/leave lifecycle, media event listeners |
| `ui/src/HomePage.re` | Landing page: create room or join by room ID |
| `ui/src/Routes.re` | Router config: `/` → HomePage, `/room/:roomId` → RoomPage |
| `ui/src/Index.re` | App entry: store hydration, React root |
| `ui/src/MediaCapture.re` | Browser media capture: `getUserMedia`, canvas frame extraction, WAV audio capture |
| `ui/src/MediaTransport.re` | Raw WebSocket messaging layer for high-frequency media data |

### Shared (Reason — dual compilation)

| File | Purpose |
|------|---------|
| `shared/js/Model.re` | State types: `Peer.t`, `Room.t`, `RemoteAudioChunk.t`, top-level `t` |
| `shared/js/Constants.re` | `event_url` (`/_events`) and `base_url` |

### Build config

| File | Purpose |
|------|---------|
| `dune` (top-level) | `(dirs server shared ui)` |
| `server/src/dune` | Server executable with `copy_files` from `ui/src/*.re` |
| `ui/src/dune` | Melange emit target + esbuild bundling rule |
| `shared/js/dune` | Melange library (client) |
| `shared/native/dune` | Native library (server) — copies `.re` sources from `shared/js/` |

## Key Design Decisions

### Per-peer subscription channels

Each client subscribes to a channel equal to their `client_id`. This lets the server send messages to individual peers via `send_to_peer(peer_id, message)` instead of broadcasting everything to the whole room. The server stores a handler per channel in the adapter, and `Middleware.broadcast` sends to all websockets subscribed to that channel (typically just one per `client_id`).

### Dual JSON encoding for local vs. remote actions

The server re-broadcasts media and toggle actions with the sender's `peer_id` added to the payload. On the client, `action_of_json` distinguishes local vs. remote by checking for a `peer_id` field:

```reason
| "toggle_video" =>
  switch (StoreJson.optionalField(~json=payload, ~fieldName="peer_id", ...)) {
  | Some(peer_id) => RemoteToggleVideo({...peer_id...})
  | None => ToggleVideo({...})
  }
```

Local actions (`ToggleVideo`) go through the mutation pipeline; remote patches (`RemoteToggleVideo`) are decoded via the strategy and applied as state updates.

### Optimistic action replay and JoinRoom

The store uses `Synced.Define` which replays pending optimistic actions on top of confirmed state. When `JoinRoom` is dispatched, it creates a room with empty peers. The `SyncPeers` patch arrives and sets the correct peers. But if `JoinRoom` is still pending during optimistic replay, it would reset peers back to empty. The reducer handles this by preserving existing peers when the room already matches:

```reason
| JoinRoom(payload) =>
  let room = {
    ...
    peers: switch (state.room) {
    | Some(room) when room.id == payload.room_id => room.peers
    | _ => [||]
    },
  };
```

### No database, no SQL

Unlike the ecommerce demo which uses annotated SQL files and PostgreSQL triggers, this demo uses a pure in-memory `VideoChat_adapter` with OCaml hashtables. This means:
- Room state is lost on server restart
- No persistence or crash recovery
- Simpler setup — no Docker/PostgreSQL needed

### Lifecycle-tied Event Listeners

Media event handling uses component-scoped listeners via `Events.listen` and `Events.unlisten`:

```reason
React.useEffect0(() => {
  let listenerId =
    VideoChatStore.Events.listen(event => {
      switch (event) {
      | MediaEvent(json) =>
        // Decode and handle media frames/audio
        let frameData =
          StoreJson.optionalField(
            ~json=payload,
            ~fieldName="frame_data",
            ~decode=Melange_json.Primitives.string_of_json,
          );
        switch (frameData) {
        | Some(data) =>
          switch (remoteCanvasRef.current) {
          | Some(canvas) => drawVideoFrame(canvas, data)
          | None => ()
          }
        | None => ()
        };
      | _ => ()
      }
    });
  Some(() => VideoChatStore.Events.unlisten(listenerId));
});
```

This pattern:
- Prevents memory leaks by unlistening on component unmount
- Avoids stale callbacks by scoping listeners to component lifecycle
- Receives typed `store_event` variants rather than raw JSON
- Uses `MediaEvent` for video/audio frames bypassing the state pipeline

### Connection Handle Hook

Outgoing media transport uses the `onConnectionHandle` hook to receive the websocket connection handle:

```reason
hooks:
  Some({
    StoreBuilder.Sync.onActionError: Some(onActionError),
    onActionAck: None,
    onCustom: None,
    onMedia: None,
    onError:
      Some((~dispatch) => message => {
        Js.Console.error("[VideoChatStore] Server error: " ++ message);
        dispatch(ResetJoinStatus);
      }),
    onOpen: Some((~dispatch) => dispatch(ResetJoinStatus)),
    onConnectionHandle:
      Some(handle => MediaTransport.setHandle(Some(handle))),
  }),
```

The connection handle allows `MediaTransport` to send raw WebSocket messages outside the store action pipeline for high-frequency media data.

## Plan: Adding Real Media Streams

### Development Setup

For the best development experience, run the dune builder in watch mode and the server with auto-restart in separate terminals:

```bash
# Terminal 1: Build watch
dune build @app --root demos/video-chat --watch

# Terminal 2: Run server with auto-restart
watchexec -w _build/default/demos/video-chat ./server.exe
```

With this setup, edits to source files trigger automatic rebuilds and server restarts. Just edit files and refresh the browser to see changes.

### Goal

Replace the static "Waiting" placeholders with live video and audio. Use the store mutation pipeline for **signaling** (join/leave, toggle) but **bypass state** for high-frequency media frames to avoid killing framerate.

### Problem: Why Media Can't Go Through Store State

At 30fps, each JPEG frame is ~50-100KB. Storing this in state means:
- **Sending**: Every frame becomes an action → IndexedDB ledger write → WebSocket mutation → server ack → ledger update. At 30fps this overwhelms the action pipeline.
- **Receiving**: Every frame triggers a reducer → new state object → Tilia reactive update → React re-render → IndexedDB confirmed-state persistence. This kills framerate.

### Architecture: Hybrid Signaling + Raw Media Transport

```
                          Store Pipeline (slow path)          Raw WS (fast path)
                          ──────────────────────────          ──────────────────
Signaling (join/leave,    dispatch() → mutation → ack         (not used)
  toggle video/audio)     → patch → reducer → state

Media (video/audio        (not used)                          RealtimeClient.Socket.sendFrame
  frames at 30fps)                                             → server rebroadcast
                                                               → Events.listen handler
                                                               → side-effect only
                                                               (set img.src / play
                                                                audio, NOT into state)
```

- **Signaling** uses the normal store dispatch/patch pipeline — low frequency, needs state tracking
- **Media** uses `RealtimeClient.Socket.sendFrame` to send raw JSON directly on the WebSocket, bypassing the action ledger. On receive, the `Events.listen` handler fires a side-effect (set `img.src`, play audio) instead of storing in state.

### Package Change

No package changes needed. `RealtimeClient.Socket.sendFrame` was already exposed as a module-level `let` binding — it just wasn't being used outside the module. It sends a raw string on the WebSocket and returns `true` if sent.

### Current State

- `MediaCapture.re` captures video frames (JPEG data URLs at ~30fps) and audio chunks (WAV data URLs)
- `MediaTransport.re` has `sendMediaFrame` / `sendMediaAudio` — uses `sendRaw` which calls `RealtimeClient.Socket.sendFrame(~handle, ~frame)`
- `RoomPage.re` renders video elements with lifecycle-tied media event listeners
- Server `handle_mutation` already handles `video_frame` and `audio_chunk` — rebroadcasts with `peer_id`
- `VideoChatStore.re` uses `Synced.Define` with a custom strategy that filters client-only actions from patch decoding
- `Model.t` has `remote_peer_id`, `remote_video_enabled`, `remote_audio_enabled` fields for UI state

### Step 1: Add media event listener in RoomPage

**File**: `ui/src/RoomPage.re`

The store's `Synced.Define` runtime fires `MediaEvent` variants for incoming media messages. Listen for these in a useEffect with proper cleanup:

```reason
React.useEffect0(() => {
  let listenerId =
    VideoChatStore.Events.listen(event => {
      switch (event) {
      | MediaEvent(json) =>
        // Media messages are wrapped: {"type": "media", "payload": {...}}
        let payload =
          StoreJson.optionalField(~json, ~fieldName="payload", ~decode=value => value);
        let actualPayload =
          switch (payload) {
          | Some(p) => p
          | None => json
          };
        // Handle frame_data for video
        let frameData =
          StoreJson.optionalField(
            ~json=actualPayload,
            ~fieldName="frame_data",
            ~decode=Melange_json.Primitives.string_of_json,
          );
        switch (frameData, peerId, roomId) {
        | (Some(data), Some(_pid), Some(_rid)) =>
          switch (remoteCanvasRef.current) {
          | Some(canvas) => drawVideoFrame(canvas, data)
          | None => ()
          };
        | _ => ()
        };
        // Handle chunk_data for audio
        let chunkData =
          StoreJson.optionalField(
            ~json=actualPayload,
            ~fieldName="chunk_data",
            ~decode=Melange_json.Primitives.string_of_json,
          );
        switch (chunkData) {
        | Some(data) => playAudioChunk(data)
        | None => ()
        };
      | _ => ()
      }
    });
  Some(() => VideoChatStore.Events.unlisten(listenerId));
});
```

### Step 2: MediaTransport implementation

**File**: `ui/src/MediaTransport.re`

The transport layer manages its own connection handle reference, which is set by the store runtime when a connection is established:

```reason
/* Transport-specific handle ref. This is NOT singleton active connection state;
   it's the transport layer's local reference to the current connection handle.
   The handle is owned by the store runtime and set via setHandle when connected. */
let handleRef: ref(option(RealtimeClient.Socket.connection_handle)) = ref(None);

/* Set the current connection handle. Called by store runtime on connect. */
let setHandle = (handle: option(RealtimeClient.Socket.connection_handle)) => {
  handleRef := handle;
};

/* Send a raw JSON payload through the store's websocket connection */
let sendRaw = dict => {
  let frame = Js.Json.stringify(Js.Json.object_(dict));
  switch (handleRef.contents) {
  | Some(handle) =>
    let _ = RealtimeClient.Socket.sendFrame(~handle, ~frame);
    ()
  | None => ()
  };
};
```

Media frame and audio sending build JSON payloads and send via `sendRaw`:

```reason
let sendMediaFrame = (roomId, peerId, frameData) => {
  /* Deduplication to avoid sending identical frames */
  switch (lastFrameSent.contents) {
  | Some(last) when last == frameData => ()
  | _ =>
    lastFrameSent := Some(frameData);
    let payload = Js.Dict.empty();
    Js.Dict.set(payload, "room_id", Js.Json.string(roomId));
    Js.Dict.set(payload, "peer_id", Js.Json.string(peerId));
    Js.Dict.set(payload, "frame_data", Js.Json.string(frameData));
    let msg = Js.Dict.empty();
    Js.Dict.set(msg, "type", Js.Json.string("media"));
    Js.Dict.set(msg, "payload", Js.Json.object_(payload));
    sendRaw(msg);
  };
};

let sendMediaAudio = (roomId, peerId, chunkData) => {
  let payload = Js.Dict.empty();
  Js.Dict.set(payload, "room_id", Js.Json.string(roomId));
  Js.Dict.set(payload, "peer_id", Js.Json.string(peerId));
  Js.Dict.set(payload, "chunk_data", Js.Json.string(chunkData));
  let msg = Js.Dict.empty();
  Js.Dict.set(msg, "type", Js.Json.string("media"));
  Js.Dict.set(msg, "payload", Js.Json.object_(payload));
  sendRaw(msg);
};
```

Key points:
- The connection handle is owned by the store runtime and passed to the transport via `setHandle`
- `sendRaw` stringifies the JSON payload and sends it via `RealtimeClient.Socket.sendFrame(~handle, ~frame)`
- This bypasses the IndexedDB action ledger entirely for high-frequency media data
- Media frames use `"type": "media"` wrapper; server `handle_mutation` handles the rebroadcast

Server-side handling of media messages:
```json
{"type": "media", "payload": {"room_id": "...", "peer_id": "...", "frame_data": "..."}}
```

### Step 3: Wire MediaCapture into RoomPage

**File**: `ui/src/RoomPage.re`

- Add `useEffect` that calls `MediaCapture.create()` on mount (when in a room)
- Call `MediaCapture.attachVideoElement()` to attach the local stream to a `<video>` element
- Call `MediaCapture.startCapture()` with callbacks that call `MediaTransport.sendMediaFrame` and `MediaTransport.sendMediaAudio` (raw WS, not store dispatch)
- Call `MediaCapture.stopCapture()` on cleanup
- Add `useRef` for a local `<video>` element

### Step 4: Display remote video via side-effect (not state)

**File**: `ui/src/RoomPage.re`

- Add a `useRef` for a remote `<canvas>` element (JPEG data URLs render via `drawVideoFrame`)
- In the media handler callback (from Step 1), call `drawVideoFrame(canvas, frameData)` directly
- Do NOT store `remote_frame_data` in state — the canvas element is the display buffer

### Step 5: Play remote audio via side-effect (not state)

**File**: `ui/src/RoomPage.re`

- In the media handler callback, call `playAudioChunk(chunkData)` to create an `Audio` element from the WAV data URL and `.play()`
- Use a simple queue to avoid gaps between chunks
- Do NOT store `remote_audio_chunk` in state

### Step 6: Remove media from state, keep signaling

**File**: `shared/js/Model.re`, `ui/src/VideoChatStore.re`

- Keep `remote_peer_id`, `remote_video_enabled`, `remote_audio_enabled` in state (for UI toggle indicators)
- The reducer only handles signaling actions (join/leave, toggle, peers)
- Media frames bypass state entirely via `Events.listen` side-effects

### Step 7: Wire toggle controls

**File**: `ui/src/RoomPage.re`

- Add mute/unmute buttons that call `MediaCapture.setVideoEnabled` / `setAudioEnabled` (local track control)
- Also dispatch `VideoChatStore.toggleVideo` / `toggleAudio` to notify remote peers (these are low-frequency, go through store pipeline)
- Remote peer's reducer handles `RemoteToggleVideo` / `RemoteToggleAudio` to update UI state

### Performance Notes

- Base64 JPEG frames over JSON: ~50-100KB per frame at 30fps = 1.5-3 MB/s bandwidth
- Raw send bypasses IndexedDB writes, action ledger, and ack processing
- Raw receive bypasses reducer, Tilia reactivity, React re-render, and state persistence
- The canvas element is the display buffer — `drawVideoFrame` triggers browser-native JPEG decode
- For production, WebRTC data channels or binary WebSocket would be needed

### Future: WebRTC Upgrade

For production quality, the signaling server would stay the same but media would flow peer-to-peer:

1. Use the store mutation pipeline for **signaling only** (SDP offer/answer exchange, ICE candidate exchange)
2. New action types: `SdpOffer`, `SdpAnswer`, `IceCandidate`
3. Server rebroadcasts these to the target peer (already supports this pattern)
4. Client creates `RTCPeerConnection`, sets remote SDP, adds ICE candidates
5. Media flows directly between browsers — no server relay needed

This requires no changes to the `reason-realtime` package — just new action types in `VideoChatStore.re` and new handlers in `server.ml`.
