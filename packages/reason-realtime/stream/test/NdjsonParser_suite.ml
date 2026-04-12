let assert_eq_int ~expected ~actual msg =
  if expected <> actual then
    Alcotest.fail (Printf.sprintf "%s: expected %d, got %d" msg expected actual)

let suite =
  ( "NdjsonParser",
    [
      Alcotest.test_case "parses full lines" `Quick (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "{\"a\":1}\n{\"b\":2}" in
          assert_eq_int ~expected:2 ~actual:(Array.length results) "count");
      Alcotest.test_case "handles partial line across chunks" `Quick (fun () ->
          let parser = NdjsonParser.make () in
          let _ = NdjsonParser.feed parser "{\"a\":" in
          let results = NdjsonParser.feed parser "1}\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count");
      Alcotest.test_case "ignores empty lines and trailing newline" `Quick (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "\n{\"x\":1}\n\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count");
      Alcotest.test_case "ignores malformed JSON lines" `Quick (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "{\"bad\n{\"good\":1}\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count");
    ] )
