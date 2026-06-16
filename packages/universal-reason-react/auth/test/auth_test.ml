let test_initial_state_is_anonymous () =
  let auth = Auth.make () in
  Alcotest.(check bool) "anonymous by default" false (Auth.is_authenticated auth);
  Alcotest.(check (option string)) "no user id by default" None
    (Option.map (fun u -> u.Auth.id) (Auth.user auth));
  Alcotest.(check (option string)) "no user role by default" None
    (Option.map (fun u -> u.Auth.role) (Auth.user auth))

let test_make_with_initial_user () =
  let user = {Auth.id = "u1"; role = "admin"} in
  let auth = Auth.make ~initial:(Auth.Authenticated user) () in
  Alcotest.(check bool) "authenticated when initial user" true (Auth.is_authenticated auth);
  Alcotest.(check (option string)) "user id" (Some "u1")
    (Option.map (fun u -> u.Auth.id) (Auth.user auth));
  Alcotest.(check (option string)) "user role" (Some "admin")
    (Option.map (fun u -> u.Auth.role) (Auth.user auth))

let test_set_state () =
  let auth = Auth.make () in
  let user = {Auth.id = "u2"; role = "distributor"} in
  Auth.set_state auth (Auth.Authenticated user);
  Alcotest.(check bool) "authenticated after set" true (Auth.is_authenticated auth);
  Alcotest.(check bool) "has matching role" true (Auth.has_role auth "distributor");
  Alcotest.(check bool) "does not have other role" false (Auth.has_role auth "admin")

let test_update () =
  let user = {Auth.id = "u3"; role = "executive"} in
  let auth = Auth.make ~initial:(Auth.Authenticated user) () in
  Auth.update auth (fun _ -> Auth.Anonymous);
  Alcotest.(check bool) "anonymous after update" false (Auth.is_authenticated auth);
  Alcotest.(check bool) "no role when anonymous" false (Auth.has_role auth "executive")

let test_has_role_when_anonymous () =
  let auth = Auth.make () in
  Alcotest.(check bool) "anonymous has no role" false (Auth.has_role auth "admin")

let test_signal_returns_state () =
  let auth = Auth.make () in
  let signal = Auth.signal auth in
  Alcotest.(check bool) "signal initially anonymous" false
    (match signal.StoreSignal.get () with
     | Auth.Anonymous -> false
     | Auth.Authenticated _ -> true);
  let user = {Auth.id = "u4"; role = "surgeon"} in
  Auth.set_state auth (Auth.Authenticated user);
  Alcotest.(check bool) "signal reflects update" true
    (match signal.StoreSignal.get () with
     | Auth.Anonymous -> false
     | Auth.Authenticated _ -> true)

let () =
  Alcotest.run "auth"
    [
      ( "state"
      , [
          Alcotest.test_case "initial state is anonymous" `Quick test_initial_state_is_anonymous;
          Alcotest.test_case "make with initial user" `Quick test_make_with_initial_user;
          Alcotest.test_case "set_state" `Quick test_set_state;
          Alcotest.test_case "update" `Quick test_update;
          Alcotest.test_case "has_role when anonymous" `Quick test_has_role_when_anonymous;
          Alcotest.test_case "signal returns state" `Quick test_signal_returns_state;
        ] );
    ]
