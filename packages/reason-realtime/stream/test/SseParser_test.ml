open Test_framework

let assert_eq_int ~expected ~actual msg =
  if expected <> actual then
    fail (Printf.sprintf "%s: expected %d, got %d" msg expected actual)
  else
    pass ()

let assert_eq_string ~expected ~actual msg =
  if expected <> actual then
    fail (Printf.sprintf "%s: expected %S, got %S" msg expected actual)
  else
    pass ()

let init () =
  describe "SseParser" (fun () ->
      test "parses basic event" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count");

      test "multi-line data joins with newline" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: hello\ndata: world\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count"
          |> fun _ ->
          assert_eq_string
            ~expected:"hello\nworld"
            ~actual:(SseParser.(events.(0).data))
            "data");

      test "event with id and type" (fun () ->
          let buffer = ref "" in
          let events =
            SseParser.parseChunk "id: 1\nevent: update\ndata: hello\n\n" ~buffer
          in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count"
          |> fun _ ->
          let evt = events.(0) in
          assert_eq_string ~expected:"hello" ~actual:evt.SseParser.data "data"
          |> fun _ ->
          assert_eq_string
            ~expected:"update"
            ~actual:(match evt.SseParser.event with Some s -> s | None -> "")
            "event"
          |> fun _ ->
          assert_eq_string
            ~expected:"1"
            ~actual:(match evt.SseParser.id with Some s -> s | None -> "")
            "id");

      test "comment ignored" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk ": comment\ndata: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count"
          |> fun _ ->
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");

      test "partial chunk completes event" (fun () ->
          let buffer = ref "" in
          let _ = SseParser.parseChunk "data: hel" ~buffer in
          let events = SseParser.parseChunk "lo\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count"
          |> fun _ ->
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");

      test "multiple events in one chunk" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: a\n\ndata: b\n\n" ~buffer in
          assert_eq_int ~expected:2 ~actual:(Array.length events) "event count"
          |> fun _ ->
          assert_eq_string ~expected:"a" ~actual:(events.(0)).SseParser.data "first data"
          |> fun _ ->
          assert_eq_string ~expected:"b" ~actual:(events.(1)).SseParser.data "second data");

      test "unknown field ignored" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "unknown: value\ndata: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count"
          |> fun _ ->
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");

      test "empty data event ignored" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "id: 1\n\n" ~buffer in
          assert_eq_int ~expected:0 ~actual:(Array.length events) "event count");

      test "events with only id and no data are ignored" (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "event: ping\nid: 2\n\n" ~buffer in
          assert_eq_int ~expected:0 ~actual:(Array.length events) "event count"))
