open PokerTypes;

type status = {
  available: bool,
  model: string,
};

type probability = {
  action: string,
  value: float,
};

type decision = {
  kind: string,
  amount: int,
  model: string,
  confidence: float,
  probabilities: array(probability),
};

type cancel = unit => unit;

[@melange_only]
let getStatus: ((status => unit), (unit => unit)) => cancel = [%raw {|
function(onStatus, onUnavailable) {
  let cancelled = false;
  const controller = new AbortController();
  const timeout = setTimeout(function() {
    if (!cancelled) {
      cancelled = true;
      try { controller.abort(); } catch (_) {}
      onUnavailable();
    }
  }, 8000);
  fetch("/api/poker/jev/status", {
    method: "GET",
    headers: {"Accept": "application/json"},
    signal: controller.signal,
  }).then(response => {
    if (!response.ok) throw new Error("status");
    return response.json();
  }).then(data => {
    if (cancelled) return;
    clearTimeout(timeout);
    if (!data || typeof data.available !== "boolean" ||
        typeof data.model !== "string" || data.model.length < 1 || data.model.length > 128) {
      onUnavailable();
      return;
    }
    onStatus({available: data.available, model: data.model});
  }).catch(_ => {
    if (!cancelled) {
      clearTimeout(timeout);
      onUnavailable();
    }
  });
  return function() {
    cancelled = true;
    clearTimeout(timeout);
    try { controller.abort(); } catch (_) {}
  };
}
|}];

[@native_only]
let getStatus = (_onStatus, _onUnavailable) => () => ();

[@melange_only]
let postDecision: (string, string, decision => unit, unit => unit) => cancel = [%raw {|
function(url, body, onDecision, onError) {
  let cancelled = false;
  let settled = false;
  const controller = new AbortController();
  let decisionTimer = null;
  const timeout = setTimeout(function() {
    if (!settled && !cancelled) {
      settled = true;
      try { controller.abort(); } catch (_) {}
      onError();
    }
  }, 20000);
  function finish() {
    settled = true;
    clearTimeout(timeout);
    if (decisionTimer !== null) {
      clearTimeout(decisionTimer);
      decisionTimer = null;
    }
  }
  fetch(url, {
    method: "POST",
    headers: {"Accept": "application/json", "Content-Type": "application/json"},
    body: body,
    signal: controller.signal,
  }).then(response => {
    if (!response.ok) throw new Error("decision");
    return response.json();
  }).then(data => {
    if (cancelled || settled) return;
    const action = data && data.action;
    const kind = action && action.kind;
    const amount = action && action.amount;
    const direct = kind === "fold" || kind === "check" || kind === "call" || kind === "all_in";
    const sized = kind === "bet" || kind === "raise";
    const probabilities = data && data.probabilities;
    if (!data || data.source !== "model" || !action || (!direct && !sized) ||
        (sized && (!Number.isInteger(amount) || amount < 0)) ||
        typeof data.model !== "string" || data.model.length < 1 || data.model.length > 128 ||
        typeof data.confidence !== "number" || !Number.isFinite(data.confidence) ||
        data.confidence < 0 || data.confidence > 1 ||
        !probabilities || typeof probabilities !== "object" || Array.isArray(probabilities)) {
      finish();
      onError();
      return;
    }
    const summary = [];
    for (const [name, value] of Object.entries(probabilities)) {
      if (typeof value !== "number" || !Number.isFinite(value) || value < 0 || value > 1) {
        finish();
        onError();
        return;
      }
      summary.push({action: name, value: value});
    }
    if (summary.length === 0) {
      finish();
      onError();
      return;
    }
    const decision = {
      kind: kind,
      amount: sized ? amount : 0,
      model: data.model,
      confidence: data.confidence,
      probabilities: summary,
    };
    decisionTimer = setTimeout(function() {
      if (cancelled || settled) return;
      finish();
      onDecision(decision);
    }, 1500);
  }).catch(_ => {
    if (!cancelled && !settled) {
      finish();
      onError();
    }
  });
  return function() {
    cancelled = true;
    clearTimeout(timeout);
    if (decisionTimer !== null) {
      clearTimeout(decisionTimer);
      decisionTimer = null;
    }
    try { controller.abort(); } catch (_) {}
  };
}
|}];

[@native_only]
let postDecision = (_url, _body, _onDecision, _onError) => () => ();

let jsonObject = fields => {
  let dict = Js.Dict.empty();
  fields->Js.Array.forEach(~f=((key, value)) => Js.Dict.set(dict, key, value));
  Js.Json.object_(dict);
};

let string = value => Js.Json.string(value);
let int = value => Js.Json.number(float_of_int(value));
let bool = value => Js.Json.boolean(value);
let list = values => Js.Json.array(values);

let suitName = suit =>
  switch (suit) {
  | Clubs => "clubs"
  | Diamonds => "diamonds"
  | Hearts => "hearts"
  | Spades => "spades"
  };

let cardJson = (card: card) =>
  jsonObject([|("rank", int(card.rank)), ("suit", string(suitName(card.suit)))|]);

let streetName = street =>
  switch (street) {
  | Preflop => "preflop"
  | Flop => "flop"
  | Turn => "turn"
  | River => "river"
  | Lobby => "lobby"
  | Showdown => "showdown"
  };

let personality = index =>
  switch (index mod 3) {
  | 0 => "cautious"
  | 1 => "balanced"
  | _ => "assertive"
  };

let publicPlayerJson = (player: player) =>
  jsonObject([|
    ("seat", int(player.id)),
    ("name", string(player.name)),
    ("stack", int(player.chips)),
    ("street_committed", int(player.streetCommitted)),
    ("total_committed", int(player.committed)),
    ("folded", bool(player.folded)),
    ("all_in", bool(player.allIn)),
    ("busted", bool(player.busted)),
    ("has_acted", bool(player.hasActed)),
    ("can_raise", bool(player.canRaise)),
    ("last_action", string(player.lastAction)),
  |]);

let legalOptionsJson = (options: legalOptions) =>
  jsonObject([|
    ("can_fold", bool(options.canFold)),
    ("can_check", bool(options.canCheck)),
    ("can_call", bool(options.canCall)),
    ("call_amount", int(options.callAmount)),
    ("can_bet", bool(options.canBet)),
    ("min_bet", int(options.minBet)),
    ("max_bet", int(options.maxBet)),
    ("can_raise", bool(options.canRaise)),
    ("min_raise_to", int(options.minRaiseTo)),
    ("max_raise_to", int(options.maxRaiseTo)),
    ("can_all_in", bool(options.canAllIn)),
  |]);

let snapshot = (game: game, index: int) => {
  let acting = game.players[index];
  let actingJson =
    jsonObject([|
      ("seat", int(index)),
      ("hole_cards", list(acting.holeCards->Js.Array.map(~f=cardJson))),
      ("stack", int(acting.chips)),
      ("street_committed", int(acting.streetCommitted)),
      ("total_committed", int(acting.committed)),
      ("personality", string(personality(index))),
    |]);
  let snapshotJson =
    jsonObject([|
      ("hand_number", int(game.handNumber)),
      ("acting_seat", int(index)),
      ("street", string(streetName(game.street))),
      ("pot", int(PokerEngine.potTotal(game))),
      ("current_bet", int(game.currentBet)),
      ("min_raise", int(game.minRaise)),
      ("dealer", int(game.dealer)),
      ("small_blind", int(game.smallBlind)),
      ("big_blind", int(game.bigBlind)),
      ("board", list(game.board->Js.Array.map(~f=cardJson))),
      ("acting_player", actingJson),
      ("players", list(game.players->Js.Array.map(~f=publicPlayerJson))),
      ("history", list(game.history->Js.Array.map(~f=string))),
    |]);
  jsonObject([|
    ("snapshot", snapshotJson),
    ("legal_options", legalOptionsJson(PokerEngine.legalOptions(game, index))),
  |])->Js.Json.stringify;
};
