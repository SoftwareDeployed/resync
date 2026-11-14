open PokerTypes

let protocol_version = 1
let max_intent_bytes = 4096

type opponent_mode = Jev | Local

type intent =
  | Deal
  | Restart
  | Set_mode of opponent_mode
  | Act of action

let suit_name = function
  | Clubs -> "clubs"
  | Diamonds -> "diamonds"
  | Hearts -> "hearts"
  | Spades -> "spades"

let street_name = function
  | Lobby -> "lobby"
  | Preflop -> "preflop"
  | Flop -> "flop"
  | Turn -> "turn"
  | River -> "river"
  | Showdown -> "showdown"

let card_json card =
  `Assoc [ ("rank", `Int card.rank); ("suit", `String (suit_name card.suit)) ]

let cards_json cards = `List (Array.to_list (Array.map card_json cards))

let player_json ~public ~game index (player : player) =
  let reveal =
    (not public) || index = PokerEngine.humanIndex
    || (game.handOver && game.street = Showdown && not player.folded && not player.busted)
  in
  `Assoc
    [ ("id", `Int player.id);
      ("name", `String player.name);
      ("chips", `Int player.chips);
      ("holeCards", if reveal then cards_json player.holeCards else `List []);
      ("holeCardCount", `Int (Array.length player.holeCards));
      ("folded", `Bool player.folded);
      ("allIn", `Bool player.allIn);
      ("busted", `Bool player.busted);
      ("committed", `Int player.committed);
      ("streetCommitted", `Int player.streetCommitted);
      ("hasActed", `Bool player.hasActed);
      ("canRaise", `Bool player.canRaise);
      ("lastAction", `String player.lastAction) ]

let game_json ~public (game : game) =
  `Assoc
    [ ("players", `List (Array.to_list (Array.mapi (player_json ~public ~game) game.players)));
      ("dealer", `Int game.dealer);
      ("smallBlind", `Int game.smallBlind);
      ("bigBlind", `Int game.bigBlind);
      ("currentPlayer", match game.currentPlayer with Some index -> `Int index | None -> `Null);
      ("currentBet", `Int game.currentBet);
      ("minRaise", `Int game.minRaise);
      ("street", `String (street_name game.street));
      ("board", cards_json game.board);
      ("deck", if public then `List [] else cards_json game.deck);
      ("nextCard", `Int (if public then 0 else game.nextCard));
      ("handNumber", `Int game.handNumber);
      ("handOver", `Bool game.handOver);
      ("history", `List (Array.to_list (Array.map (fun entry -> `String entry) game.history)));
      ("result", `String game.result);
      ("rngSeed", `Int (if public then 0 else game.rngSeed)) ]

let public_game_json game = game_json ~public:true game

(* This private serialization is used only as an exact stale-work guard. It is
   deliberately separate from every message constructor sent to a browser. *)
let native_snapshot game = Yojson.Safe.to_string (game_json ~public:false game)

let envelope kind fields = `Assoc (("version", `Int protocol_version) :: ("type", `String kind) :: fields)
let message json = Yojson.Safe.to_string json

let state_message game = message (envelope "state" [ ("game", public_game_json game) ])

let mode_name = function Jev -> "jev" | Local -> "local"

let status_message ~available ~model mode =
  message
    (envelope "status"
       [ ("available", `Bool available);
         ("model", `String model);
         ("opponentMode", `String (mode_name mode)) ])

let thinking_message ~seat ~hand_number ~street ~model =
  message
    (envelope "thinking"
       [ ("seat", `Int seat);
         ("handNumber", `Int hand_number);
         ("street", `String (street_name street));
         ("model", `String model) ])

let jev_decision_message ~seat ~hand_number ~street ~action_label decision =
  message
    (envelope "jevDecision"
       [ ("seat", `Int seat);
         ("handNumber", `Int hand_number);
         ("street", `String (street_name street));
         ("actionLabel", `String action_label);
         ("decision", JevDecision.decision_to_json decision) ])

let error_message text = message (envelope "error" [ ("message", `String text) ])

let strip_trailing_slash value =
  let length = String.length value in
  if length > 0 && value.[length - 1] = '/' then String.sub value 0 (length - 1) else value

let origin_allowed ~origin ~host =
  match origin, host with
  | None, _ -> true
  | Some _, None -> false
  | Some origin, Some host ->
      let origin = String.lowercase_ascii (strip_trailing_slash origin) in
      let host = String.lowercase_ascii host in
      if origin = "http://" ^ host || origin = "https://" ^ host then true
      else if String.ends_with ~suffix:":80" host then
        origin = "http://" ^ String.sub host 0 (String.length host - 3)
      else if String.ends_with ~suffix:":443" host then
        origin = "https://" ^ String.sub host 0 (String.length host - 4)
      else false

let personality index =
  match index mod 3 with 0 -> "cautious" | 1 -> "balanced" | _ -> "assertive"

let jev_card (card : PokerTypes.card) : JevDecision.card =
  { rank = card.rank; suit = suit_name card.suit }

let decision_request (game : game) index =
  let acting = game.players.(index) in
  let options = PokerEngine.legalOptions game index in
  let acting_player : JevDecision.acting_player =
    { seat = index;
      hole_cards = Array.to_list (Array.map jev_card acting.holeCards);
      stack = acting.chips;
      street_committed = acting.streetCommitted;
      total_committed = acting.committed;
      personality = personality index }
  in
  let players =
    Array.to_list
      (Array.map
         (fun (player : player) ->
            JevDecision.
              { seat = player.id;
                name = player.name;
                stack = player.chips;
                street_committed = player.streetCommitted;
                total_committed = player.committed;
                folded = player.folded;
                all_in = player.allIn;
                busted = player.busted;
                has_acted = player.hasActed;
                can_raise = player.canRaise;
                last_action = player.lastAction })
         game.players)
  in
  let snapshot : JevDecision.snapshot =
    { hand_number = game.handNumber;
      acting_seat = index;
      street = street_name game.street;
      pot = PokerEngine.potTotal game;
      current_bet = game.currentBet;
      min_raise = game.minRaise;
      dealer = game.dealer;
      small_blind = game.smallBlind;
      big_blind = game.bigBlind;
      board = Array.to_list (Array.map jev_card game.board);
      acting_player;
      players;
      history = Array.to_list game.history }
  in
  let legal_options : JevDecision.legal_options =
    { can_fold = options.canFold;
      can_check = options.canCheck;
      can_call = options.canCall;
      call_amount = options.callAmount;
      can_bet = options.canBet;
      min_bet = options.minBet;
      max_bet = options.maxBet;
      can_raise = options.canRaise;
      min_raise_to = options.minRaiseTo;
      max_raise_to = options.maxRaiseTo;
      can_all_in = options.canAllIn }
  in
  JevDecision.{ snapshot; legal_options }

let action_of_decision (decision : JevDecision.decision) =
  match decision.kind, decision.amount with
  | "fold", None -> Ok Fold
  | "check", None -> Ok Check
  | "call", None -> Ok Call
  | "bet", Some amount -> Ok (Bet amount)
  | "raise", Some amount -> Ok (RaiseTo amount)
  | "all_in", None -> Ok AllIn
  | _ -> Error "Jev returned an incomplete poker action."

let invalid reason = Error ("Invalid poker stream message: " ^ reason)

let exact_fields allowed fields =
  List.length fields = List.length allowed
  && List.for_all (fun name -> List.mem_assoc name fields) allowed

let string_field name fields =
  match List.assoc_opt name fields with Some (`String value) -> Ok value | _ -> invalid (name ^ " must be a string")

let int_field name fields =
  match List.assoc_opt name fields with Some (`Int value) -> Ok value | _ -> invalid (name ^ " must be an integer")

let parse_action = function
  | `Assoc fields ->
      let allowed = if List.mem_assoc "amount" fields then [ "kind"; "amount" ] else [ "kind" ] in
      if not (exact_fields allowed fields) then invalid "action has unknown or missing fields"
      else
        (match string_field "kind" fields with
         | Error error -> Error error
         | Ok "fold" when allowed = [ "kind" ] -> Ok Fold
         | Ok "check" when allowed = [ "kind" ] -> Ok Check
         | Ok "call" when allowed = [ "kind" ] -> Ok Call
         | Ok "all_in" when allowed = [ "kind" ] -> Ok AllIn
         | Ok ("bet" as kind) | Ok ("raise" as kind) ->
             (match int_field "amount" fields with
              | Error error -> Error error
              | Ok amount when amount >= 0 -> Ok (if kind = "bet" then Bet amount else RaiseTo amount)
              | Ok _ -> invalid "action amount must not be negative")
         | Ok _ -> invalid "unknown action kind or invalid amount")
  | _ -> invalid "action must be an object"

let parse_intent body =
  if String.length body = 0 || String.length body > max_intent_bytes then
    invalid "message size must be between 1 and 4096 bytes"
  else
    try
      match Yojson.Safe.from_string body with
      | `Assoc fields ->
          (match int_field "version" fields with
           | Error error -> Error error
           | Ok version when version <> protocol_version -> invalid "unsupported protocol version"
           | Ok _ ->
               (match string_field "type" fields with
                | Error error -> Error error
                | Ok "deal" ->
                    if exact_fields [ "version"; "type" ] fields then Ok Deal else invalid "deal has unknown fields"
                | Ok "restart" ->
                    if exact_fields [ "version"; "type" ] fields then Ok Restart else invalid "restart has unknown fields"
                | Ok "mode" ->
                    if not (exact_fields [ "version"; "type"; "mode" ] fields) then invalid "mode has unknown or missing fields"
                    else
                      (match string_field "mode" fields with
                       | Ok "jev" -> Ok (Set_mode Jev)
                       | Ok "local" -> Ok (Set_mode Local)
                       | Ok _ -> invalid "mode must be jev or local"
                       | Error error -> Error error)
                | Ok "action" ->
                    if not (exact_fields [ "version"; "type"; "action" ] fields) then invalid "action intent has unknown or missing fields"
                    else
                      (match List.assoc_opt "action" fields with
                       | Some json -> (match parse_action json with Ok action -> Ok (Act action) | Error error -> Error error)
                       | None -> invalid "missing action")
                | Ok _ -> invalid "unknown message type"))
      | _ -> invalid "message must be an object"
    with Yojson.Json_error _ -> invalid "malformed JSON"
