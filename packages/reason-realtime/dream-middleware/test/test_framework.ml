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
  (match !current_suite_ref with Some s -> suites_ref := !suites_ref @ [ s ] | None -> ());
  current_suite_ref := None

let test name fn =
  match !current_suite_ref with
  | Some suite -> suite.tests <- suite.tests @ [ { name; run = fn } ]
  | None -> Printf.eprintf "test called outside of describe\n"

let pass () = Passed
let fail message = Failed message

let run_all () =
  let failed_tests = ref 0 in
  Printf.printf "\n=== Dream Middleware Tests ===\n\n";
  List.iter
    (fun suite ->
      Printf.printf "Suite: %s\n" suite.name;
      List.iter
        (fun test_case ->
          match test_case.run () with
          | Passed -> Printf.printf "  [PASS] %s\n" test_case.name
          | Failed message ->
              incr failed_tests;
              Printf.printf "  [FAIL] %s: %s\n" test_case.name message)
        suite.tests)
    !suites_ref;
  if !failed_tests > 0 then 1 else 0
