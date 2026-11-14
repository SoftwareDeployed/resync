open PokerTypes

let get_ok = function Ok value -> value | Error message -> Alcotest.fail message
let dealt () = get_ok (PokerEngine.startHand (PokerEngine.createGame ~seed:42 ()))

let public_state_hides_private_data () =
  let game = dealt () in
  let json = PokerProtocol.public_game_json game in
  let open Yojson.Safe.Util in
  Alcotest.check Alcotest.int "no deck cards" 0 (member "deck" json |> to_list |> List.length);
  Alcotest.check Alcotest.int "no deck cursor" 0 (member "nextCard" json |> to_int);
  Alcotest.check Alcotest.int "no RNG seed" 0 (member "rngSeed" json |> to_int);
  let players = member "players" json |> to_list in
  Alcotest.check Alcotest.int "human cards visible" 2
    (List.hd players |> member "holeCards" |> to_list |> List.length);
  List.tl players
  |> List.iter (fun player ->
         Alcotest.check Alcotest.int "opponent cards hidden" 0
           (member "holeCards" player |> to_list |> List.length);
         Alcotest.check Alcotest.int "card-back count remains public" 2
           (member "holeCardCount" player |> to_int))

let showdown_reveals_only_contenders () =
  let game = dealt () in
  let players = Array.copy game.players in
  players.(1) <- { players.(1) with folded = true };
  let showdown = { game with players; street = Showdown; handOver = true } in
  let json = PokerProtocol.public_game_json showdown in
  let open Yojson.Safe.Util in
  let public_players = member "players" json |> to_list |> Array.of_list in
  Alcotest.check Alcotest.int "folded opponent remains hidden" 0
    (public_players.(1) |> member "holeCards" |> to_list |> List.length);
  Alcotest.check Alcotest.int "showdown contender revealed" 2
    (public_players.(2) |> member "holeCards" |> to_list |> List.length)

let native_snapshot_is_complete () =
  let game = dealt () in
  let private_snapshot = PokerProtocol.native_snapshot game in
  let public_message = PokerProtocol.state_message game in
  Alcotest.check Alcotest.bool "private guard differs from public state" true
    (private_snapshot <> public_message);
  Alcotest.check Alcotest.bool "exact native snapshot is stable" true
    (private_snapshot = PokerProtocol.native_snapshot game)

let parses_versioned_action_intents () =
  (match PokerProtocol.parse_intent
          {|{"version":1,"type":"action","action":{"kind":"raise","amount":80}}|} with
  | Ok (PokerProtocol.Act (RaiseTo 80)) -> ()
  | Ok _ -> Alcotest.fail "wrong action intent"
  | Error message -> Alcotest.fail message);
  Alcotest.check Alcotest.bool "rejects negative sizing" true
    (Result.is_error
       (PokerProtocol.parse_intent
          {|{"version":1,"type":"action","action":{"kind":"bet","amount":-1}}|}));
  Alcotest.check Alcotest.bool "rejects unknown fields" true
    (Result.is_error
       (PokerProtocol.parse_intent
          {|{"version":1,"type":"action","action":{"kind":"fold"},"deck":[]}|}));
  Alcotest.check Alcotest.bool "rejects unsupported versions" true
    (Result.is_error (PokerProtocol.parse_intent {|{"version":2,"type":"deal"}|}))

let builds_request_from_native_state () =
  let game = dealt () in
  let acting = Option.get game.currentPlayer in
  let request = PokerProtocol.decision_request game acting in
  Alcotest.check Alcotest.int "native acting seat" acting request.snapshot.acting_seat;
  Alcotest.check Alcotest.int "only acting private cards supplied" 2
    (List.length request.snapshot.acting_player.hole_cards);
  Alcotest.check Alcotest.int "native pot" (PokerEngine.potTotal game) request.snapshot.pot;
  let body = JevDecision.upstream_body ~model:"jev-latest" request in
  let open Yojson.Safe.Util in
  let state = Yojson.Safe.from_string body |> member "state" in
  Alcotest.check Alcotest.bool "no deck upstream" true (member "deck" state = `Null);
  member "public_players" state |> to_list
  |> List.iter (fun player ->
         Alcotest.check Alcotest.bool "public player has no hole cards" true
           (member "hole_cards" player = `Null))

let () =
  Alcotest.run "poker stream protocol"
    [ ("privacy", [
        Alcotest.test_case "public state hides private data" `Quick public_state_hides_private_data;
        Alcotest.test_case "showdown reveals only contenders" `Quick showdown_reveals_only_contenders;
        Alcotest.test_case "native snapshot remains private and exact" `Quick native_snapshot_is_complete;
      ]);
      ("intents", [Alcotest.test_case "parses versioned action intents" `Quick parses_versioned_action_intents]);
      ("Jev adapter", [Alcotest.test_case "builds request from native state" `Quick builds_request_from_native_state]);
    ]
