open Melange_json.Primitives;

module ChatMessage = {
  [@deriving json]
  type t = {
    id: string,
    sender_id: string,
    text: string,
    sent_at: float,
  };
};

module Peer = {
  [@deriving json]
  type t = {
    id: string,
    joined_at: float,
  };
};

module Room = {
  [@deriving json]
  type t = {
    id: string,
    created_at: float,
    peers: array(Peer.t),
  };
};

module RemoteAudioChunk = {
  [@deriving json]
  type t = {
    peer_id: string,
    chunk_data: string,
  };
};

[@deriving json]
type t = {
  client_id: string,
  room: option(Room.t),
  is_joined: bool,
  local_video_enabled: bool,
  local_audio_enabled: bool,
  remote_peer_id: option(string),
  remote_video_enabled: bool,
  remote_audio_enabled: bool,
  messages: array(ChatMessage.t),
  updated_at: float,
};
