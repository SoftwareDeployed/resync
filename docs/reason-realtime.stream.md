# reason-realtime/stream

Streaming helpers for universal Reason React apps and Dream servers.

## What this package does

This package provides the small primitives behind request/response streaming:

- client-side streaming pipelines
- SSE and NDJSON parsing
- Dream/Lwt stream broadcasting on the server
- simple event writers for server push

## Public modules

### `StreamPipe` (`packages/reason-realtime/stream/src/StreamPipe.re`)

Core pipeline type and combinators:

- `type t('a)`
- `make`
- `subscribe`
- `map`
- `filterMap`
- `tap`
- `toPromise`

Use this when you need to fan out or transform a stream of events before it reaches UI state.

### `StreamPipeFetch` (`packages/reason-realtime/stream/src/StreamPipeFetch.re`)

Browser fetch transport for streaming responses.

- `type fetch_event`
- `post`

The JS implementation uses `fetch` plus `AbortController`; the native build is a stub so the package stays dual-target safe.

### `SseParser` (`packages/reason-realtime/stream/src/SseParser.re`)

Server-sent-event parsing helpers.

- `type event`
- `parseChunk`

### `NdjsonParser` (`packages/reason-realtime/stream/src/NdjsonParser.re`)

NDJSON parsing helpers for token streams and other line-delimited responses.

- `type t`
- `make`
- `feed`

### `StreamPipeDream` (`packages/reason-realtime/stream/native/StreamPipeDream.re`)

Dream-side stream integration.

- `type t('a)`
- `fromStream`
- `fromLwtStream`
- `broadcast`
- `broadcast_with_lwt`

### `SseWriter` (`packages/reason-realtime/stream/native/SseWriter.re`)

Server-side SSE output helper.

- `writeEvent`

## Streaming lifecycle

1. The server produces chunks or events.
2. `NdjsonParser` / `SseParser` turns raw bytes into structured events.
3. `StreamPipe` transforms and filters the event stream.
4. Dream helpers broadcast or write events to clients.
5. The client consumes the stream and updates UI state.

## Concrete examples

### NDJSON token stream in `llm-chat`

`demos/llm-chat/server/src/server.ml` parses Ollama’s streaming response one chunk at a time:

```reason
let ndjson_parser = NdjsonParser.make ();
let* () =
  broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_started"
    [("thread_id", `String thread_id); ("message_id", `String assistant_message_id)];

let rec read_loop () =
  let* chunk = Lwt_stream.get body_stream in
  match chunk with
  | None ->
      broadcast_stream_event ~broadcast_fn ~thread_id ~event:"stream_complete"
        [("thread_id", `String thread_id); ("message_id", `String assistant_message_id)];
      finalize_assistant_message ~request ~thread_id ~assistant_message_id ~full_response
  | Some chunk ->
      let jsons = NdjsonParser.feed ndjson_parser chunk in
      Array.iter (fun json -> (* extract token text and broadcast token_received *) ) jsons;
      read_loop ()
```

Use `NdjsonParser.feed` whenever the upstream server emits newline-delimited JSON instead of one JSON object per response.

### SSE parsing and writing

`SseParser.parseChunk` accumulates raw text until it sees an empty line separator. `SseWriter.writeEvent` emits the matching SSE wire format:

```reason
let buffer = ref("");
let events = SseParser.parseChunk("event: token\ndata: hello\n\n", ~buffer);

let* () =
  SseWriter.writeEvent(stream, ~event=Some("token"), ~id=Some("1"), ~data="hello", ());
```

That pairing is the smallest useful model: parse chunks on the way in, write `event:` / `data:` lines on the way out.

### StreamPipe composition

`StreamPipe` is intentionally tiny. The real value is in composition:

```reason
let pipe =
  StreamPipe.make(~subscribe=next => {
    next("one");
    next("two");
    ()
  })
  |> StreamPipe.map(value => String.uppercase_ascii(value))
  |> StreamPipe.filterMap(value => if (value == "") { None } else { Some(value) });
```

This is the pattern used by the parser tests and by Dream integration: build a source pipe, transform it, then subscribe the final consumer.

## Usage example

The `llm-chat` demo uses `NdjsonParser` in `demos/llm-chat/server/src/server.ml` to consume streaming Ollama responses.

## Tests

- `packages/reason-realtime/stream/test/StreamPipe_suite.ml`
- `packages/reason-realtime/stream/test/SseParser_suite.ml`
- `packages/reason-realtime/stream/test/NdjsonParser_suite.ml`

## Source index

- `packages/reason-realtime/stream/src/StreamPipe.re`
- `packages/reason-realtime/stream/src/StreamPipeFetch.re`
- `packages/reason-realtime/stream/src/SseParser.re`
- `packages/reason-realtime/stream/src/NdjsonParser.re`
- `packages/reason-realtime/stream/native/StreamPipeDream.re`
- `packages/reason-realtime/stream/native/SseWriter.re`
