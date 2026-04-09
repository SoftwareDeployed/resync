open Melange_json.Primitives;

[@deriving json]
type state = Model.t;
type subscription = string;

type join_room_payload = {
  room_id: string,
  peer_id: string,
};

type toggle_video_payload = {
  room_id: string,
  peer_id: string,
  enabled: bool,
};

type toggle_audio_payload = {
  room_id: string,
  peer_id: string,
  enabled: bool,
};

type peer_left_payload = {
  room_id: string,
  peer_id: string,
};

type peer_joined_payload = {
  room_id: string,
  peer: Model.Peer.t,
};

type remote_toggle_media_payload = {
  room_id: string,
  peer_id: string,
  enabled: bool,
};

type action =
  | JoinRoom(join_room_payload)
  | LeaveRoom(peer_left_payload)
  | ToggleVideo(toggle_video_payload)
  | ToggleAudio(toggle_audio_payload)
  | PeerJoined(peer_joined_payload)
  | PeerLeft(peer_left_payload)
  | RemoteToggleVideo(remote_toggle_media_payload)
  | RemoteToggleAudio(remote_toggle_media_payload)
  | ResetJoinStatus
  | JoinRoomAcknowledged;

type store = {
  room_id: string,
  state,
  peers_count: int,
};

let emptyState: state = {
  client_id: UUID.make(),
  room: None,
  is_joined: false,
  local_video_enabled: true,
  local_audio_enabled: true,
  remote_peer_id: None,
  remote_video_enabled: true,
  remote_audio_enabled: true,
  updated_at: Js.Date.now(),
};

let storeName = "video-chat";

let scopeKeyOfState = (state: state) => state.client_id;

let timestampOfState = (state: state) => state.updated_at;

let setTimestamp = (~state: state, ~timestamp: float) => {
  ...state,
  updated_at: timestamp,
};

let action_to_json = action =>
  switch (action) {
  | JoinRoom(payload) =>
    StoreJson.parse(
      "{\"kind\":\"join_room\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ ",\"peer_id\":"
      ++ string_to_json(payload.peer_id)->Melange_json.to_string
      ++ "}}",
    )
  | LeaveRoom(payload) =>
    StoreJson.parse(
      "{\"kind\":\"leave_room\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ ",\"peer_id\":"
      ++ string_to_json(payload.peer_id)->Melange_json.to_string
      ++ "}}",
    )
  | PeerJoined(payload) =>
    StoreJson.parse(
      "{\"kind\":\"peer_joined\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ "}}",
    )
  | PeerLeft(payload) =>
    StoreJson.parse(
      "{\"kind\":\"peer_left\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ ",\"peer_id\":"
      ++ string_to_json(payload.peer_id)->Melange_json.to_string
      ++ "}}",
    )
  | ToggleVideo(payload) =>
    StoreJson.parse(
      "{\"kind\":\"toggle_video\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ ",\"peer_id\":"
      ++ string_to_json(payload.peer_id)->Melange_json.to_string
      ++ ",\"enabled\":"
      ++ bool_to_json(payload.enabled)->Melange_json.to_string
      ++ "}}",
    )
  | ToggleAudio(payload) =>
    StoreJson.parse(
      "{\"kind\":\"toggle_audio\",\"payload\":{"
      ++ "\"room_id\":"
      ++ string_to_json(payload.room_id)->Melange_json.to_string
      ++ ",\"peer_id\":"
      ++ string_to_json(payload.peer_id)->Melange_json.to_string
      ++ ",\"enabled\":"
      ++ bool_to_json(payload.enabled)->Melange_json.to_string
      ++ "}}",
    )
  | RemoteToggleVideo(_)
  | RemoteToggleAudio(_) =>
    StoreJson.parse("{\"kind\":\"noop\",\"payload\":{}}")
  | ResetJoinStatus =>
    StoreJson.parse("{\"kind\":\"noop\",\"payload\":{}}")
  | JoinRoomAcknowledged =>
    StoreJson.parse("{\"kind\":\"noop\",\"payload\":{}}")
  };

let action_of_json = json => {
  let kind =
    StoreJson.requiredField(~json, ~fieldName="kind", ~decode=string_of_json);
  let payload =
    StoreJson.requiredField(~json, ~fieldName="payload", ~decode=value =>
      value
    );
  switch (kind) {
  | "join_room" =>
    JoinRoom({
      room_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="room_id",
          ~decode=string_of_json,
        ),
      peer_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="peer_id",
          ~decode=string_of_json,
        ),
    })
  | "leave_room" =>
    LeaveRoom({
      room_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="room_id",
          ~decode=string_of_json,
        ),
      peer_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="peer_id",
          ~decode=string_of_json,
        ),
    })
  | "toggle_video" =>
    switch (
      StoreJson.optionalField(
        ~json=payload,
        ~fieldName="peer_id",
        ~decode=string_of_json,
      )
    ) {
    | Some(peer_id) =>
      RemoteToggleVideo({
        room_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="room_id",
            ~decode=string_of_json,
          ),
        peer_id,
        enabled:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="enabled",
            ~decode=bool_of_json,
          ),
      })
    | None =>
      ToggleVideo({
        room_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="room_id",
            ~decode=string_of_json,
          ),
        peer_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="peer_id",
            ~decode=string_of_json,
          ),
        enabled:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="enabled",
            ~decode=bool_of_json,
          ),
      })
    }
  | "toggle_audio" =>
    switch (
      StoreJson.optionalField(
        ~json=payload,
        ~fieldName="peer_id",
        ~decode=string_of_json,
      )
    ) {
    | Some(peer_id) =>
      RemoteToggleAudio({
        room_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="room_id",
            ~decode=string_of_json,
          ),
        peer_id,
        enabled:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="enabled",
            ~decode=bool_of_json,
          ),
      })
    | None =>
      ToggleAudio({
        room_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="room_id",
            ~decode=string_of_json,
          ),
        peer_id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="peer_id",
            ~decode=string_of_json,
          ),
        enabled:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="enabled",
            ~decode=bool_of_json,
          ),
      })
    }
  | "peer_joined" =>
    PeerJoined({
      room_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="room_id",
          ~decode=string_of_json,
        ),
      peer: {
        id:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="peer_id",
            ~decode=string_of_json,
          ),
        joined_at:
          StoreJson.requiredField(
            ~json=payload,
            ~fieldName="joined_at",
            ~decode=float_of_json,
          ),
      },
    })
  | "peer_left" =>
    PeerLeft({
      room_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="room_id",
          ~decode=string_of_json,
        ),
      peer_id:
        StoreJson.requiredField(
          ~json=payload,
          ~fieldName="peer_id",
          ~decode=string_of_json,
        ),
    })
  | _ =>
    JoinRoom({
      room_id: "",
      peer_id: "",
    })
  };
};

let upsertPeer = (peers: array(Model.Peer.t), peer: Model.Peer.t) => {
  let remaining =
    peers->Js.Array.filter(~f=(existing: Model.Peer.t) =>
      existing.id != peer.id
    );
  Js.Array.concat(~other=[|peer|], remaining);
};

let removePeer = (peers: array(Model.Peer.t), peer_id: string) =>
  peers->Js.Array.filter(~f=(peer: Model.Peer.t) => peer.id != peer_id);

let firstPeerId = (peers: array(Model.Peer.t)) =>
  switch (Belt.Array.get(peers, 0)) {
  | Some(peer) => Some(peer.id)
  | None => None
  };

let resolveRemotePeerId = (~current, ~selfId, ~peers) => {
  let otherPeers =
    peers->Js.Array.filter(~f=(peer: Model.Peer.t) => peer.id != selfId);
  switch (current) {
  | Some(current_id) =>
    if (otherPeers->Js.Array.some(~f=(peer: Model.Peer.t) =>
          peer.id == current_id
        )) {
      Some(current_id);
    } else {
      firstPeerId(otherPeers);
    }
  | None => firstPeerId(otherPeers)
  };
};

let currentRoomMatches = (state: state, room_id: string) =>
  switch (state.room) {
  | Some(room) => room.id == room_id
  | None => false
  };

let reduce = (~state: state, ~action: action) => {
  let updated_at = Js.Date.now();
  switch (action) {
  | JoinRoom(payload) =>
    let alreadyInRoom =
      switch (state.room) {
      | Some(room) when room.id == payload.room_id => true
      | _ => false
      };
    let room: Model.Room.t = {
      id: payload.room_id,
      created_at: updated_at,
      peers:
        switch (state.room) {
        | Some(room) when room.id == payload.room_id => room.peers
        | _ => [||]
        },
    };
    {
      ...state,
      room: Some(room),
      remote_peer_id: alreadyInRoom ? state.remote_peer_id : None,
      remote_video_enabled: alreadyInRoom ? state.remote_video_enabled : true,
      remote_audio_enabled: alreadyInRoom ? state.remote_audio_enabled : true,
      updated_at,
    };
  | LeaveRoom(_payload) => {
      ...state,
      room: None,
      is_joined: false,
      remote_peer_id: None,
      remote_video_enabled: true,
      remote_audio_enabled: true,
      updated_at,
    }
  | ToggleVideo(payload) =>
    if (currentRoomMatches(state, payload.room_id)) {
      {
        ...state,
        local_video_enabled: payload.enabled,
        updated_at,
      };
    } else {
      state;
    }
  | ToggleAudio(payload) =>
    if (currentRoomMatches(state, payload.room_id)) {
      {
        ...state,
        local_audio_enabled: payload.enabled,
        updated_at,
      };
    } else {
      state;
    }
  | PeerJoined(payload) =>
    if (currentRoomMatches(state, payload.room_id) || state.room == None) {
      let existingPeers =
        state.room
        ->Belt.Option.map(r => r.peers)
        ->Belt.Option.getWithDefault([||]);
      let updatedPeers = upsertPeer(existingPeers, payload.peer);
      let nextRemotePeerId =
        resolveRemotePeerId(
          ~current=state.remote_peer_id,
          ~selfId=state.client_id,
          ~peers=updatedPeers,
        );
      let remotePeerChanged = nextRemotePeerId != state.remote_peer_id;
      let isOwnJoin = payload.peer.id == state.client_id;
      {
        ...state,
        room:
          Some({
            id: payload.room_id,
            created_at: updated_at,
            peers: updatedPeers,
          }),
        is_joined: isOwnJoin ? true : state.is_joined,
        remote_peer_id: nextRemotePeerId,
        remote_video_enabled:
          remotePeerChanged || nextRemotePeerId == None
            ? true : state.remote_video_enabled,
        remote_audio_enabled:
          remotePeerChanged || nextRemotePeerId == None
            ? true : state.remote_audio_enabled,
        updated_at,
      };
    } else {
      state;
    }
  | PeerLeft(payload) =>
    if (currentRoomMatches(state, payload.room_id)) {
      let updatedPeers =
        removePeer(
          state.room
          ->Belt.Option.getWithDefault({
              id: payload.room_id,
              created_at: updated_at,
              peers: [||],
            }).
            peers,
          payload.peer_id,
        );
      let nextRemotePeerId =
        resolveRemotePeerId(
          ~current=state.remote_peer_id,
          ~selfId=state.client_id,
          ~peers=updatedPeers,
        );
      let remotePeerChanged = nextRemotePeerId != state.remote_peer_id;
      {
        ...state,
        room:
          Some({
            id: payload.room_id,
            created_at: updated_at,
            peers: updatedPeers,
          }),
        remote_peer_id: nextRemotePeerId,
        remote_video_enabled:
          remotePeerChanged || nextRemotePeerId == None
            ? true : state.remote_video_enabled,
        remote_audio_enabled:
          remotePeerChanged || nextRemotePeerId == None
            ? true : state.remote_audio_enabled,
        updated_at,
      };
    } else {
      state;
    }
  | RemoteToggleVideo(payload) =>
    if (currentRoomMatches(state, payload.room_id)) {
      {
        ...state,
        remote_peer_id: Some(payload.peer_id),
        remote_video_enabled: payload.enabled,
        updated_at,
      };
    } else {
      state;
    }
  | RemoteToggleAudio(payload) =>
    if (currentRoomMatches(state, payload.room_id)) {
      {
        ...state,
        remote_peer_id: Some(payload.peer_id),
        remote_audio_enabled: payload.enabled,
        updated_at,
      };
    } else {
      state;
    }
  | ResetJoinStatus => {
      ...state,
      is_joined: false,
      updated_at,
    }
  | JoinRoomAcknowledged => {
      ...state,
      is_joined: true,
      updated_at,
    }
  };
};

[@platform js]
let onActionError = message => {
  Sonner.showError(message);
  ReasonReactRouter.push("/");
};

[@platform native]
let onActionError = _message => ();

/* ============================================================================
   New Grouped API (Task 7) - Using Synced.Define with custom strategy
   ============================================================================ */

module StoreDef =
  StoreBuilder.Synced.Define({
    type nonrec state = state;
    type nonrec action = action;
    type nonrec store = store;
    type nonrec subscription = subscription;
    type patch = action;

    let base: StoreBuilder.Synced.baseConfig(state, action, store, subscription) = {
      storeName,
      emptyState,
      reduce,
      state_of_json,
      state_to_json,
      action_of_json,
      action_to_json,
      makeStore:
        (~state: state, ~derive: option(Tilia.Core.deriver(store))=?, ()) => {
        let room_id =
          switch (state.room) {
          | Some(room) => room.id
          | None => ""
          };
        {
          room_id,
          state,
          peers_count:
            StoreBuilder.derived(
              ~derive?,
              ~client=
                (store: store) =>
                  switch (store.state.room) {
                  | Some(room) => Array.length(room.peers)
                  | None => 0
                  },
              ~server=
                () =>
                  switch (state.room) {
                  | Some(room) => Array.length(room.peers)
                  | None => 0
                  },
              (),
            ),
        };
      },
      scopeKeyOfState,
      timestampOfState,
      setTimestamp,
      transport: {
        subscriptionOfState: (state: state): option(subscription) =>
          Some(state.client_id),
        encodeSubscription: (sub: subscription) => sub,
        eventUrl: Constants.event_url,
        baseUrl: Constants.base_url,
      },
      stateElementId: Some("initial-store"),
      hooks:
        Some({
          StoreBuilder.Sync.onActionError: Some(onActionError),
          onActionAck: None,
          onCustom: None,
          onMedia: None,
          onError:
            Some((~dispatch) => (message) => {
              Js.Console.error("[VideoChatStore] Server error: " ++ message);
              dispatch(ResetJoinStatus);
            }),
          onOpen: Some((~dispatch) => dispatch(ResetJoinStatus)),
          onConnectionHandle:
            Some((handle) => MediaTransport.setHandle(Some(handle))),
        }),
    };

    let strategy: StoreBuilder.Sync.customStrategy(state, patch) =
      StoreBuilder.Sync.custom(
        ~decodePatch=json =>
          switch (action_of_json(json)) {
          | JoinRoom(_)
          | LeaveRoom(_)
          | ToggleVideo(_)
          | ToggleAudio(_)
          | ResetJoinStatus
          | JoinRoomAcknowledged => None
          | patch => Some(patch)
          },
        ~updateOfPatch=(patch, state) => reduce(~state, ~action=patch),
      );
  });

include (
  StoreDef:
    StoreBuilder.Runtime.Exports with
      type state := state and type action := action and type t := store
);

type t = store;

module Context = StoreDef.Context;

let dispatch = StoreDef.dispatch;

let joinRoom = (store: t, room_id: string) => {
  let peer_id = store.state.client_id;
  Js.Console.log(
    "[joinRoom] called with room_id=" ++ room_id ++ " peer_id=" ++ peer_id,
  );
  dispatch(
    JoinRoom({
      room_id,
      peer_id,
    }),
  );
  peer_id;
};

let leaveRoom = (store: t) =>
  switch (store.state.room) {
  | Some(room) =>
    dispatch(
      LeaveRoom({
        room_id: room.id,
        peer_id: store.state.client_id,
      }),
    )
  | None => ()
  };

let sendVideoFrame = (store: t, frame_data: string) =>
  switch (store.state.room) {
  | Some(room) when store.state.is_joined =>
    MediaTransport.sendMediaFrame(room.id, store.state.client_id, frame_data)
  | _ => ()
  };

let sendAudioChunk = (store: t, chunk_data: string) =>
  switch (store.state.room) {
  | Some(room) when store.state.is_joined =>
    MediaTransport.sendMediaAudio(room.id, store.state.client_id, chunk_data)
  | _ => ()
  };

let toggleVideo = (store: t, enabled: bool) =>
  switch (store.state.room) {
  | Some(room) =>
    dispatch(
      ToggleVideo({
        room_id: room.id,
        peer_id: store.state.client_id,
        enabled,
      }),
    )
  | None => ()
  };

let toggleAudio = (store: t, enabled: bool) =>
  switch (store.state.room) {
  | Some(room) =>
    dispatch(
      ToggleAudio({
        room_id: room.id,
        peer_id: store.state.client_id,
        enabled,
      }),
    )
  | None => ()
  };
