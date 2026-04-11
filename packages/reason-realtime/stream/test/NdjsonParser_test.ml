open Test_framework

let assert_eq_int ~expected ~actual msg =
  if expected <> actual then
    fail (Printf.sprintf "%s: expected %d, got %d" msg expected actual)
  else
    pass ()

let init () =
  describe "NdjsonParser" (fun () ->
      test "parses full lines" (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "{\"a\":1}\n{\"b\":2}" in
          assert_eq_int ~expected:2 ~actual:(Array.length results) "count");

      test "handles partial line across chunks" (fun () ->
          let parser = NdjsonParser.make () in
          let _ = NdjsonParser.feed parser "{\"a\":" in
          let results = NdjsonParser.feed parser "1}\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count");

      test "ignores empty lines and trailing newline" (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "\n{\"x\":1}\n\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count");

      test "ignores malformed JSON lines" (fun () ->
          let parser = NdjsonParser.make () in
          let results = NdjsonParser.feed parser "{\"bad\n{\"good\":1}\n" in
          assert_eq_int ~expected:1 ~actual:(Array.length results) "count"))
