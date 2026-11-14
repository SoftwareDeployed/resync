exception Invalid_payload

let invalid () = raise Invalid_payload
let max_chips = 1_000_000_000
let max_body_bytes = 32_768

type card = { rank : int; suit : string }

type acting_player = {
  seat : int;
  hole_cards : card list;
  stack : int;
  street_committed : int;
  total_committed : int;
  personality : string;
}

type public_player = {
  seat : int;
  name : string;
  stack : int;
  street_committed : int;
  total_committed : int;
  folded : bool;
  all_in : bool;
  busted : bool;
  has_acted : bool;
  can_raise : bool;
  last_action : string;
}

type legal_options = {
  can_fold : bool;
  can_check : bool;
  can_call : bool;
  call_amount : int;
  can_bet : bool;
  min_bet : int;
  max_bet : int;
  can_raise : bool;
  min_raise_to : int;
  max_raise_to : int;
  can_all_in : bool;
}

type snapshot = {
  hand_number : int;
  acting_seat : int;
  street : string;
  pot : int;
  current_bet : int;
  min_raise : int;
  dealer : int;
  small_blind : int;
  big_blind : int;
  board : card list;
  acting_player : acting_player;
  players : public_player list;
  history : string list;
}

type request = { snapshot : snapshot; legal_options : legal_options }

type probability = { action : string; value : float }

type decision = {
  kind : string;
  amount : int option;
  model : string;
  confidence : float;
  probabilities : probability list;
}

let fields allowed = function
  | `Assoc fields ->
      if
        List.length fields <> List.length allowed
        || not
             (List.for_all
                (fun name -> List.mem_assoc name fields)
                allowed)
      then invalid ();
      fields
  | _ -> invalid ()

let field name fields =
  match List.assoc_opt name fields with Some value -> value | None -> invalid ()

let string_value ?(min_length = 0) ~max_length = function
  | `String value
    when String.length value >= min_length && String.length value <= max_length ->
      value
  | _ -> invalid ()

let string_field ?(min_length = 0) ~max_length name fields =
  field name fields |> string_value ~min_length ~max_length

let int_value ?(minimum = 0) ?(maximum = max_chips) = function
  | `Int value when value >= minimum && value <= maximum -> value
  | _ -> invalid ()

let int_field ?(minimum = 0) ?(maximum = max_chips) name fields =
  field name fields |> int_value ~minimum ~maximum

let bool_field name fields =
  match field name fields with `Bool value -> value | _ -> invalid ()

let list_value = function `List values -> values | _ -> invalid ()
let list_field name fields = field name fields |> list_value

let parse_card = function
  | `Assoc _ as json ->
      let fields = fields [ "rank"; "suit" ] json in
      let rank = int_field ~minimum:2 ~maximum:14 "rank" fields in
      let suit = string_field ~min_length:1 ~max_length:8 "suit" fields in
      if not (List.mem suit [ "clubs"; "diamonds"; "hearts"; "spades" ]) then
        invalid ();
      { rank; suit }
  | _ -> invalid ()

let parse_acting_player json =
  let fields =
    fields
      [ "seat"; "hole_cards"; "stack"; "street_committed"; "total_committed";
        "personality" ]
      json
  in
  let hole_cards = list_field "hole_cards" fields |> List.map parse_card in
  if List.length hole_cards <> 2 then invalid ();
  let personality = string_field ~min_length:1 ~max_length:16 "personality" fields in
  if not (List.mem personality [ "cautious"; "balanced"; "assertive" ]) then
    invalid ();
  {
    seat = int_field ~minimum:1 ~maximum:5 "seat" fields;
    hole_cards;
    stack = int_field "stack" fields;
    street_committed = int_field "street_committed" fields;
    total_committed = int_field "total_committed" fields;
    personality;
  }

let parse_public_player json =
  let fields =
    fields
      [ "seat"; "name"; "stack"; "street_committed"; "total_committed";
        "folded"; "all_in"; "busted"; "has_acted"; "can_raise"; "last_action" ]
      json
  in
  {
    seat = int_field ~maximum:5 "seat" fields;
    name = string_field ~min_length:1 ~max_length:32 "name" fields;
    stack = int_field "stack" fields;
    street_committed = int_field "street_committed" fields;
    total_committed = int_field "total_committed" fields;
    folded = bool_field "folded" fields;
    all_in = bool_field "all_in" fields;
    busted = bool_field "busted" fields;
    has_acted = bool_field "has_acted" fields;
    can_raise = bool_field "can_raise" fields;
    last_action = string_field ~max_length:80 "last_action" fields;
  }

let parse_legal_options json =
  let fields =
    fields
      [ "can_fold"; "can_check"; "can_call"; "call_amount"; "can_bet";
        "min_bet"; "max_bet"; "can_raise"; "min_raise_to"; "max_raise_to";
        "can_all_in" ]
      json
  in
  let options =
    {
      can_fold = bool_field "can_fold" fields;
      can_check = bool_field "can_check" fields;
      can_call = bool_field "can_call" fields;
      call_amount = int_field "call_amount" fields;
      can_bet = bool_field "can_bet" fields;
      min_bet = int_field "min_bet" fields;
      max_bet = int_field "max_bet" fields;
      can_raise = bool_field "can_raise" fields;
      min_raise_to = int_field "min_raise_to" fields;
      max_raise_to = int_field "max_raise_to" fields;
      can_all_in = bool_field "can_all_in" fields;
    }
  in
  if options.can_bet && options.can_raise then invalid ();
  if not
       (options.can_fold || options.can_check || options.can_call
       || options.can_bet || options.can_raise || options.can_all_in)
  then invalid ();
  if options.can_bet && (options.min_bet <= 0 || options.min_bet > options.max_bet) then invalid ();
  if options.can_raise && (options.min_raise_to <= 0 || options.min_raise_to > options.max_raise_to) then invalid ();
  options

let parse_snapshot json =
  let fields =
    fields
      [ "hand_number"; "acting_seat"; "street"; "pot"; "current_bet";
        "min_raise"; "dealer"; "small_blind"; "big_blind"; "board";
        "acting_player"; "players"; "history" ]
      json
  in
  let street = string_field ~min_length:1 ~max_length:8 "street" fields in
  if not (List.mem street [ "preflop"; "flop"; "turn"; "river" ]) then invalid ();
  let board = list_field "board" fields |> List.map parse_card in
  let expected_board_cards =
    match street with
    | "preflop" -> 0
    | "flop" -> 3
    | "turn" -> 4
    | "river" -> 5
    | _ -> invalid ()
  in
  if List.length board <> expected_board_cards then invalid ();
  let acting_player = field "acting_player" fields |> parse_acting_player in
  let players = list_field "players" fields |> List.map parse_public_player in
  if List.length players <> 6 then invalid ();
  let seats = List.map (fun (player : public_player) -> player.seat) players in
  if List.length (List.sort_uniq compare seats) <> 6 then invalid ();
  let acting_seat = int_field ~minimum:1 ~maximum:5 "acting_seat" fields in
  if acting_player.seat <> acting_seat then invalid ();
  let public_acting =
    List.find_opt (fun (player : public_player) -> player.seat = acting_seat) players
  in
  (match public_acting with
  | Some player
    when player.stack = acting_player.stack
         && player.street_committed = acting_player.street_committed
         && player.total_committed = acting_player.total_committed -> ()
  | _ -> invalid ());
  let visible_cards = board @ acting_player.hole_cards in
  if List.length (List.sort_uniq compare visible_cards) <> List.length visible_cards then
    invalid ();
  let history =
    list_field "history" fields
    |> List.map (string_value ~max_length:180)
  in
  if List.length history > 24 then invalid ();
  let pot = int_field "pot" fields in
  let committed_total =
    List.fold_left
      (fun total (player : public_player) -> total + player.total_committed)
      0 players
  in
  if committed_total <> pot then invalid ();
  {
    hand_number = int_field ~minimum:1 ~maximum:1_000_000_000 "hand_number" fields;
    acting_seat;
    street;
    pot;
    current_bet = int_field "current_bet" fields;
    min_raise = int_field "min_raise" fields;
    dealer = int_field ~maximum:5 "dealer" fields;
    small_blind = int_field ~maximum:5 "small_blind" fields;
    big_blind = int_field ~maximum:5 "big_blind" fields;
    board;
    acting_player;
    players;
    history;
  }

let parse_request body =
  try
    if String.length body > max_body_bytes then invalid ();
    let json = Yojson.Safe.from_string body in
    let root = fields [ "snapshot"; "legal_options" ] json in
    let snapshot = field "snapshot" root |> parse_snapshot in
    let legal_options = field "legal_options" root |> parse_legal_options in
    if snapshot.current_bet = 0 && legal_options.can_raise then invalid ();
    if snapshot.current_bet > 0 && legal_options.can_bet then invalid ();
    Ok { snapshot; legal_options }
  with Invalid_payload | Yojson.Json_error _ ->
    Error "Invalid poker decision payload."

let card_json card =
  `Assoc [ ("rank", `Int card.rank); ("suit", `String card.suit) ]

let public_player_json (player : public_player) =
  `Assoc
    [ ("seat", `Int player.seat);
      ("name", `String player.name);
      ("stack", `Int player.stack);
      ("street_committed", `Int player.street_committed);
      ("total_committed", `Int player.total_committed);
      ("folded", `Bool player.folded);
      ("all_in", `Bool player.all_in);
      ("busted", `Bool player.busted);
      ("has_acted", `Bool player.has_acted);
      ("can_raise", `Bool player.can_raise);
      ("last_action", `String player.last_action) ]

let legal_options_json (options : legal_options) =
  `Assoc
    [ ("can_fold", `Bool options.can_fold);
      ("can_check", `Bool options.can_check);
      ("can_call", `Bool options.can_call);
      ("call_amount", `Int options.call_amount);
      ("can_bet", `Bool options.can_bet);
      ("min_bet", `Int options.min_bet);
      ("max_bet", `Int options.max_bet);
      ("can_raise", `Bool options.can_raise);
      ("min_raise_to", `Int options.min_raise_to);
      ("max_raise_to", `Int options.max_raise_to);
      ("can_all_in", `Bool options.can_all_in) ]

let position (snapshot : snapshot) =
  if snapshot.acting_seat = snapshot.dealer then "dealer/button"
  else if snapshot.acting_seat = snapshot.small_blind then "small blind"
  else if snapshot.acting_seat = snapshot.big_blind then "big blind"
  else
    let offset = (snapshot.acting_seat - snapshot.dealer + 6) mod 6 in
    match offset with
    | 1 -> "first after button"
    | 2 -> "second after button"
    | 3 -> "third after button"
    | 4 -> "fourth after button"
    | _ -> "fifth after button"

let action_criteria (options : legal_options) =
  let criteria = ref [] in
  let add allowed name description = if allowed then criteria := (name, `String description) :: !criteria in
  add options.can_fold "fold" "Discard this hand and contest no further chips.";
  add options.can_check "check" "Pass the action without adding chips.";
  add options.can_call "call"
    (Printf.sprintf "Match the current wager for %d chips." options.call_amount);
  add options.can_bet "bet"
    (Printf.sprintf "Open the betting with a target from %d to %d chips."
       options.min_bet options.max_bet);
  add options.can_raise "raise"
    (Printf.sprintf "Raise to a total street wager from %d to %d chips."
       options.min_raise_to options.max_raise_to);
  add options.can_all_in "all_in" "Commit all remaining chips with the all-in action.";
  List.rev !criteria

let sizing_criteria (options : legal_options) =
  if options.can_bet || options.can_raise then
    [ ("minimum", `String "Use the minimum legal bet or raise target.");
      ("half_pot", `String "Use a half-pot-sized target, clamped to the legal limits.");
      ("pot", `String "Use a pot-sized target, clamped to the legal limits.");
      ("all_in", `String "Use the maximum legal target, committing the full stack.") ]
  else
    [ ("not_applicable", `String "No bet or raise is legal for this decision.") ]

let action_names request = List.map fst (action_criteria request.legal_options)
let sizing_names request = List.map fst (sizing_criteria request.legal_options)

let upstream_body ~model request =
  let snapshot = request.snapshot in
  let acting = snapshot.acting_player in
  let state =
    `Assoc
      [ ("game", `String "Six-handed Texas Hold'em; play chips only");
        ("hand_number", `Int snapshot.hand_number);
        ("street", `String snapshot.street);
        ("pot", `Int snapshot.pot);
        ("current_bet", `Int snapshot.current_bet);
        ("min_raise", `Int snapshot.min_raise);
        ("dealer_seat", `Int snapshot.dealer);
        ("small_blind_seat", `Int snapshot.small_blind);
        ("big_blind_seat", `Int snapshot.big_blind);
        ("board", `List (List.map card_json snapshot.board));
        ("acting_player",
         `Assoc
           [ ("seat", `Int acting.seat);
             ("hole_cards", `List (List.map card_json acting.hole_cards));
             ("stack", `Int acting.stack);
             ("street_committed", `Int acting.street_committed);
             ("total_committed", `Int acting.total_committed);
             ("position", `String (position snapshot));
             ("personality", `String acting.personality) ]);
        ("public_players", `List (List.map public_player_json snapshot.players));
        ("public_action_history", `List (List.map (fun entry -> `String entry) snapshot.history));
        ("legal_options", legal_options_json request.legal_options) ]
  in
  let questions =
    `Assoc
      [ ("action",
         `Assoc
           [ ("type", `String "choice");
             ("instructions",
              `String
                "Choose exactly one legal poker action for the acting opponent. Use only the supplied public table state and this acting player's two private cards. Do not infer or request any hidden cards.");
             ("criteria", `Assoc (action_criteria request.legal_options)) ]);
        ("raise_size",
         `Assoc
           [ ("type", `String "choice");
             ("instructions",
              `String
                (if request.legal_options.can_bet || request.legal_options.can_raise then
                   "Choose the sizing for a bet or raise. This answer is required even if the action choice is fold, check, call, or all_in; sizing is applied only to bet and raise."
                 else
                   "No bet or raise is legal, so choose not_applicable."));
             ("criteria", `Assoc (sizing_criteria request.legal_options)) ]) ]
  in
  Yojson.Safe.to_string
    (`Assoc [ ("model", `String model); ("state", state); ("questions", questions) ])

let float_value = function
  | `Float value when classify_float value = FP_normal || classify_float value = FP_zero -> value
  | `Int value -> float_of_int value
  | _ -> invalid ()

let probability_value = function
  | json ->
      let value = float_value json in
      if value < 0.0 || value > 1.0 then invalid ();
      value

let clamp low high value = max low (min high value)

let sized_target ~choice (snapshot : snapshot) (options : legal_options) =
  let is_bet = options.can_bet in
  let minimum = if is_bet then options.min_bet else options.min_raise_to in
  let maximum = if is_bet then options.max_bet else options.max_raise_to in
  let target =
    match choice with
    | "minimum" -> minimum
    | "half_pot" ->
        if is_bet then snapshot.pot / 2
        else snapshot.current_bet + max snapshot.min_raise (snapshot.pot / 2)
    | "pot" ->
        if is_bet then snapshot.pot
        else snapshot.current_bet + max snapshot.min_raise snapshot.pot
    | "all_in" -> maximum
    | _ -> invalid ()
  in
  clamp minimum maximum target

let parse_response ~request body =
  try
    let root = Yojson.Safe.from_string body in
    let root_fields =
      match root with `Assoc fields -> fields | _ -> invalid ()
    in
    let model = string_field ~min_length:1 ~max_length:128 "model" root_fields in
    let answers =
      match field "answers" root_fields with
      | `Assoc fields -> fields
      | _ -> invalid ()
    in
    let choice_answer name =
      match field name answers with
      | `Assoc fields ->
          if string_field ~min_length:1 ~max_length:6 "type" fields <> "choice" then
            invalid ();
          string_field ~min_length:1 ~max_length:16 "choice" fields,
          field "confidence" fields |> probability_value
      | _ -> invalid ()
    in
    let action_choice, confidence = choice_answer "action" in
    let sizing_choice, _sizing_confidence = choice_answer "raise_size" in
    if not (List.mem action_choice (action_names request)) then invalid ();
    if not (List.mem sizing_choice (sizing_names request)) then invalid ();
    let probabilities_fields =
      match field "probabilities" (match field "action" answers with `Assoc fields -> fields | _ -> invalid ()) with
      | `Assoc fields -> fields
      | _ -> invalid ()
    in
    let probabilities =
      action_names request
      |> List.map (fun action ->
             let value = field action probabilities_fields |> probability_value in
             { action; value })
    in
    let kind, amount =
      match action_choice with
      | "bet" -> ("bet", Some (sized_target ~choice:sizing_choice request.snapshot request.legal_options))
      | "raise" -> ("raise", Some (sized_target ~choice:sizing_choice request.snapshot request.legal_options))
      | "fold" | "check" | "call" | "all_in" -> (action_choice, None)
      | _ -> invalid ()
    in
    Ok { kind; amount; model; confidence; probabilities }
  with Invalid_payload | Yojson.Json_error _ ->
    Error "Jev returned an invalid decision."

let decision_to_json decision =
  let action_fields =
    ("kind", `String decision.kind)
    :: (match decision.amount with
       | Some amount -> [ ("amount", `Int amount) ]
       | None -> [])
  in
  `Assoc
    [ ("action", `Assoc action_fields);
      ("source", `String "model");
      ("model", `String decision.model);
      ("confidence", `Float decision.confidence);
      ("probabilities",
       `Assoc
         (List.map
            (fun probability -> (probability.action, `Float probability.value))
            decision.probabilities)) ]
