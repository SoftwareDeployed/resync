let valid_payload = {|
{
  "snapshot": {
    "hand_number": 3,
    "acting_seat": 1,
    "street": "flop",
    "pot": 120,
    "current_bet": 0,
    "min_raise": 20,
    "dealer": 5,
    "small_blind": 0,
    "big_blind": 1,
    "board": [
      {"rank": 2, "suit": "diamonds"},
      {"rank": 7, "suit": "hearts"},
      {"rank": 9, "suit": "spades"}
    ],
    "acting_player": {
      "seat": 1,
      "hole_cards": [
        {"rank": 14, "suit": "clubs"},
        {"rank": 13, "suit": "clubs"}
      ],
      "stack": 980,
      "street_committed": 0,
      "total_committed": 20,
      "personality": "balanced"
    },
    "players": [
      {"seat": 0, "name": "You", "stack": 1000, "street_committed": 0, "total_committed": 0, "folded": false, "all_in": false, "busted": false, "has_acted": true, "can_raise": true, "last_action": "Check"},
      {"seat": 1, "name": "Ada", "stack": 980, "street_committed": 0, "total_committed": 20, "folded": false, "all_in": false, "busted": false, "has_acted": false, "can_raise": true, "last_action": ""},
      {"seat": 2, "name": "Babbage", "stack": 900, "street_committed": 0, "total_committed": 100, "folded": false, "all_in": false, "busted": false, "has_acted": true, "can_raise": true, "last_action": "Call 100"},
      {"seat": 3, "name": "Curie", "stack": 1000, "street_committed": 0, "total_committed": 0, "folded": false, "all_in": false, "busted": false, "has_acted": true, "can_raise": true, "last_action": "Fold"},
      {"seat": 4, "name": "Dijkstra", "stack": 1000, "street_committed": 0, "total_committed": 0, "folded": false, "all_in": false, "busted": false, "has_acted": true, "can_raise": true, "last_action": "Fold"},
      {"seat": 5, "name": "Hopper", "stack": 1000, "street_committed": 0, "total_committed": 0, "folded": false, "all_in": false, "busted": false, "has_acted": true, "can_raise": true, "last_action": "Fold"}
    ],
    "history": ["Ada checks"]
  },
  "legal_options": {
    "can_fold": true,
    "can_check": true,
    "can_call": false,
    "call_amount": 0,
    "can_bet": true,
    "min_bet": 20,
    "max_bet": 980,
    "can_raise": false,
    "min_raise_to": 40,
    "max_raise_to": 980,
    "can_all_in": true
  }
}
|}

let response action sizing = {|
{
  "model": "jev-1.14.0",
  "answers": {
    "action": {
      "type": "choice",
      "choice": "|} ^ action ^ {|",
      "confidence": 0.6,
      "probabilities": {"fold": 0.1, "check": 0.2, "bet": 0.6, "all_in": 0.1}
    },
    "raise_size": {
      "type": "choice",
      "choice": "|} ^ sizing ^ {|",
      "confidence": 0.5,
      "probabilities": {"minimum": 0.2, "half_pot": 0.5, "pot": 0.2, "all_in": 0.1}
    }
  },
  "usage": {}
}
|}

let get_ok = function
  | Ok value -> value
  | Error message -> Alcotest.fail message

let request () = get_ok (JevDecision.parse_request valid_payload)

let rejects_extra_game_state () =
  let json = Yojson.Safe.from_string valid_payload in
  match json with
  | `Assoc fields ->
      let with_deck = `Assoc (("deck", `List []) :: fields) |> Yojson.Safe.to_string in
      Alcotest.check Alcotest.bool "deck is rejected" true
        (Result.is_error (JevDecision.parse_request with_deck))
  | _ -> Alcotest.fail "expected object"

let builds_bounded_upstream_state () =
  let body = JevDecision.upstream_body ~model:"jev-latest" (request ()) in
  let json = Yojson.Safe.from_string body in
  let open Yojson.Safe.Util in
  let state = member "state" json in
  Alcotest.check Alcotest.string "configured model" "jev-latest" (member "model" json |> to_string);
  Alcotest.check Alcotest.bool "no deck" true (member "deck" state = `Null);
  Alcotest.check Alcotest.int "two acting cards" 2
    (member "acting_player" state |> member "hole_cards" |> to_list |> List.length);
  member "public_players" state |> to_list
  |> List.iter (fun player ->
         Alcotest.check Alcotest.bool "public player has no hole cards" true
           (member "hole_cards" player = `Null));
  let criteria = member "questions" json |> member "action" |> member "criteria" in
  Alcotest.check Alcotest.bool "call omitted" true (member "call" criteria = `Null);
  Alcotest.check Alcotest.bool "bet included" false (member "bet" criteria = `Null)

let maps_and_clamps_sizes () =
  let request = request () in
  let decision = get_ok (JevDecision.parse_response ~request (response "bet" "half_pot")) in
  Alcotest.check Alcotest.string "kind" "bet" decision.kind;
  Alcotest.check Alcotest.(option int) "half pot" (Some 60) decision.amount;
  Alcotest.check Alcotest.string "actual model" "jev-1.14.0" decision.model;
  let raise_request =
    JevDecision.
      { snapshot = { request.snapshot with current_bet = 100; min_raise = 50; pot = 300 };
        legal_options =
          { can_fold = true; can_check = true; can_call = false; call_amount = 0;
            can_bet = false; min_bet = 20; max_bet = 980; can_raise = true;
            min_raise_to = 150; max_raise_to = 980; can_all_in = true } }
  in
  Alcotest.check Alcotest.int "raise half pot" 250
    (JevDecision.sized_target ~choice:"half_pot" raise_request.snapshot raise_request.legal_options);
  Alcotest.check Alcotest.int "minimum clamps" 150
    (JevDecision.sized_target ~choice:"minimum" raise_request.snapshot raise_request.legal_options);
  Alcotest.check Alcotest.int "all in clamps" 980
    (JevDecision.sized_target ~choice:"all_in" raise_request.snapshot raise_request.legal_options)

let rejects_invalid_answers () =
  let request = request () in
  Alcotest.check Alcotest.bool "illegal action rejected" true
    (Result.is_error (JevDecision.parse_response ~request (response "call" "minimum")));
  Alcotest.check Alcotest.bool "missing answers rejected" true
    (Result.is_error (JevDecision.parse_response ~request "{}"))

let () =
  Alcotest.run "poker Jev decision"
    [ ("validation", [
        Alcotest.test_case "rejects extra game state" `Quick rejects_extra_game_state;
        Alcotest.test_case "builds bounded upstream state" `Quick builds_bounded_upstream_state;
      ]);
      ("mapping", [
        Alcotest.test_case "maps and clamps sizes" `Quick maps_and_clamps_sizes;
        Alcotest.test_case "rejects invalid answers" `Quick rejects_invalid_answers;
      ]) ]
