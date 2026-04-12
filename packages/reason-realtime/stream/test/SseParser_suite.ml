let assert_eq_int ~expected ~actual msg =
  if expected <> actual then
    Alcotest.fail (Printf.sprintf "%s: expected %d, got %d" msg expected actual)

let assert_eq_string ~expected ~actual msg =
  if expected <> actual then
    Alcotest.fail (Printf.sprintf "%s: expected %S, got %S" msg expected actual)

let suite =
  ( "SseParser",
    [
      Alcotest.test_case "parses basic event" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count");
      Alcotest.test_case "multi-line data joins with newline" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: hello\ndata: world\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count";
          assert_eq_string
            ~expected:"hello\nworld"
            ~actual:(SseParser.(events.(0).data))
            "data");
      Alcotest.test_case "event with id and type" `Quick (fun () ->
          let buffer = ref "" in
          let events =
            SseParser.parseChunk "id: 1\nevent: update\ndata: hello\n\n" ~buffer
          in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count";
          let evt = events.(0) in
          assert_eq_string ~expected:"hello" ~actual:evt.SseParser.data "data";
          assert_eq_string
            ~expected:"update"
            ~actual:(match evt.SseParser.event with Some s -> s | None -> "")
            "event";
          assert_eq_string
            ~expected:"1"
            ~actual:(match evt.SseParser.id with Some s -> s | None -> "")
            "id");
      Alcotest.test_case "comment ignored" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk ": comment\ndata: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count";
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");
      Alcotest.test_case "partial chunk completes event" `Quick (fun () ->
          let buffer = ref "" in
          let _ = SseParser.parseChunk "data: hel" ~buffer in
          let events = SseParser.parseChunk "lo\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count";
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");
      Alcotest.test_case "multiple events in one chunk" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "data: a\n\ndata: b\n\n" ~buffer in
          assert_eq_int ~expected:2 ~actual:(Array.length events) "event count";
          assert_eq_string ~expected:"a" ~actual:(events.(0)).SseParser.data "first data";
          assert_eq_string ~expected:"b" ~actual:(events.(1)).SseParser.data "second data");
      Alcotest.test_case "unknown field ignored" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "unknown: value\ndata: hello\n\n" ~buffer in
          assert_eq_int ~expected:1 ~actual:(Array.length events) "event count";
          assert_eq_string ~expected:"hello" ~actual:(events.(0)).SseParser.data "data");
      Alcotest.test_case "empty data event ignored" `Quick (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "id: 1\n\n" ~buffer in
          assert_eq_int ~expected:0 ~actual:(Array.length events) "event count");
      Alcotest.test_case "events with only id and no data are ignored" `Quick
        (fun () ->
          let buffer = ref "" in
          let events = SseParser.parseChunk "event: ping\nid: 2\n\n" ~buffer in
          assert_eq_int ~expected:0 ~actual:(Array.length events) "event count");
    ] )
