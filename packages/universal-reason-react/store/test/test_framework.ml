type test_result =
  | Passed
  | Failed of string

type test_case = {
  name : string;
  run : unit -> test_result;
}

type test_suite = {
  name : string;
  mutable tests : test_case list;
}

let suites_ref = ref []
let current_suite_ref = ref (None : test_suite option)

let describe name fn =
  let suite = { name; tests = [] } in
  current_suite_ref := Some suite;
  fn ();
  (match !current_suite_ref with
  | Some s -> suites_ref := !suites_ref @ [s]
  | None -> ());
  current_suite_ref := None

let test name fn =
  match !current_suite_ref with
  | Some suite ->
      let test_case = { name; run = fn } in
      suite.tests <- suite.tests @ [test_case]
  | None ->
      Printf.eprintf "test called outside of describe\n"

let pass () = Passed

let fail message = Failed message

let assert_equals ?(message = "") expected actual =
  if expected = actual then
    Passed
  else
    Failed message

let assert_true ?(message = "Expected true") value =
  if value then Passed else Failed message

let assert_false ?(message = "Expected false") value =
  if not value then Passed else Failed message

let assert_some ?(message = "Expected Some") opt =
  match opt with
  | Some _ -> Passed
  | None -> Failed message

let assert_none ?(message = "Expected None") opt =
  match opt with
  | None -> Passed
  | Some _ -> Failed message

let assert_list_length ?(message = "") expected lst =
  let actual = List.length lst in
  if expected = actual then
    Passed
  else
    let msg =
      if message = "" then
        Printf.sprintf "Expected list length %d but got %d" expected actual
      else
        message
    in
    Failed msg

let run_all () =
  let total_tests = ref 0 in
  let passed_tests = ref 0 in
  let failed_tests = ref 0 in

  Printf.printf "\n=== Store Runtime Tests ===\n\n";

  let run_test suite_name test_case =
    incr total_tests;
    let result = test_case.run () in
    match result with
    | Passed ->
        incr passed_tests;
        Printf.printf "  [PASS] %s\n" test_case.name
    | Failed msg ->
        incr failed_tests;
        Printf.printf "  [FAIL] %s: %s\n" test_case.name msg
  in

  let rec run_suite suites =
    match suites with
    | [] -> ()
    | suite :: rest ->
        Printf.printf "Suite: %s\n" suite.name;
        List.iter (run_test suite.name) suite.tests;
        run_suite rest
  in

  run_suite !suites_ref;

  Printf.printf "\n=== Results ===\n";
  Printf.printf "Total: %d\n" !total_tests;
  Printf.printf "Passed: %d\n" !passed_tests;
  Printf.printf "Failed: %d\n" !failed_tests;

  if !failed_tests > 0 then (
    Printf.printf "\nTEST RUN FAILED\n";
    1)
  else (
    Printf.printf "\nAll tests passed!\n";
    0)
