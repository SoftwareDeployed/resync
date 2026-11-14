open PokerTypes;

type status = {
  available: bool,
  model: string,
  opponentMode: string,
};

type probability = {
  action: string,
  value: float,
};

type decision = {
  model: string,
  confidence: float,
  probabilities: array(probability),
};

type decisionEvent = {
  seat: int,
  handNumber: int,
  street: string,
  actionLabel: string,
  model: string,
  confidence: float,
  probabilities: array(probability),
};

type jevDecision = {
  seat: int,
  handNumber: int,
  street,
  actionLabel: string,
  decision,
};

type rawCard = {
  rank: int,
  suit: string,
};

type rawPlayer = {
  id: int,
  name: string,
  chips: int,
  holeCards: array(rawCard),
  holeCardCount: int,
  folded: bool,
  allIn: bool,
  busted: bool,
  committed: int,
  streetCommitted: int,
  hasActed: bool,
  canRaise: bool,
  lastAction: string,
};

type rawGame = {
  players: array(rawPlayer),
  dealer: int,
  smallBlind: int,
  bigBlind: int,
  currentPlayer: int,
  currentBet: int,
  minRaise: int,
  street: string,
  board: array(rawCard),
  deck: array(rawCard),
  nextCard: int,
  handNumber: int,
  handOver: bool,
  history: array(string),
  result: string,
  rngSeed: int,
};

type t = {
  sendJson: string => unit,
  close: unit => unit,
};

[@melange_only]
let rawConnect: (
  unit => unit,
  rawGame => unit,
  status => unit,
  int => unit,
  decisionEvent => unit,
  string => unit,
  string => unit,
) => t = [%raw {|
function(onOpen, onState, onStatus, onThinking, onDecision, onServerError, onDisconnected) {
  function fail(message) { throw new Error(message); }
  function object(value, label) {
    if (!value || typeof value !== "object" || Array.isArray(value)) fail(label + " must be an object");
    return value;
  }
  function field(value, name, label) {
    object(value, label);
    if (!Object.prototype.hasOwnProperty.call(value, name)) fail(label + " is missing " + name);
    return value[name];
  }
  function integer(value, label, minimum) {
    if (!Number.isSafeInteger(value) || value < minimum) fail(label + " must be an integer >= " + minimum);
    return value;
  }
  function boolean(value, label) {
    if (typeof value !== "boolean") fail(label + " must be a boolean");
    return value;
  }
  function string(value, label, maximum) {
    if (typeof value !== "string" || value.length > maximum) fail(label + " must be a string of at most " + maximum + " characters");
    return value;
  }
  function array(value, label) {
    if (!Array.isArray(value)) fail(label + " must be an array");
    return value;
  }
  function suit(value, label) {
    string(value, label, 8);
    if (value !== "clubs" && value !== "diamonds" && value !== "hearts" && value !== "spades") fail(label + " has an unknown suit");
    return value;
  }
  function street(value, label) {
    string(value, label, 8);
    if (["lobby", "preflop", "flop", "turn", "river", "showdown"].indexOf(value) < 0) fail(label + " has an unknown street");
    return value;
  }
  function card(value, label) {
    object(value, label);
    return {
      rank: integer(field(value, "rank", label), label + ".rank", 2),
      suit: suit(field(value, "suit", label), label + ".suit"),
    };
  }
  function cards(value, label) {
    return array(value, label).map(function(item, index) {
      const result = card(item, label + "[" + index + "]");
      if (result.rank > 14) fail(label + "[" + index + "].rank must be at most 14");
      return result;
    });
  }
  function seat(value, label) {
    const result = integer(value, label, 0);
    if (result > 5) fail(label + " must be a seat from 0 to 5");
    return result;
  }
  function player(value, label) {
    object(value, label);
    const holeCards = cards(field(value, "holeCards", label), label + ".holeCards");
    const holeCardCount = integer(field(value, "holeCardCount", label), label + ".holeCardCount", 0);
    if (holeCardCount > 2 || (holeCards.length !== 0 && holeCards.length !== holeCardCount)) fail(label + " has inconsistent hidden card data");
    return {
      id: seat(field(value, "id", label), label + ".id"),
      name: string(field(value, "name", label), label + ".name", 32),
      chips: integer(field(value, "chips", label), label + ".chips", 0),
      holeCards: holeCards,
      holeCardCount: holeCardCount,
      folded: boolean(field(value, "folded", label), label + ".folded"),
      allIn: boolean(field(value, "allIn", label), label + ".allIn"),
      busted: boolean(field(value, "busted", label), label + ".busted"),
      committed: integer(field(value, "committed", label), label + ".committed", 0),
      streetCommitted: integer(field(value, "streetCommitted", label), label + ".streetCommitted", 0),
      hasActed: boolean(field(value, "hasActed", label), label + ".hasActed"),
      canRaise: boolean(field(value, "canRaise", label), label + ".canRaise"),
      lastAction: string(field(value, "lastAction", label), label + ".lastAction", 80),
    };
  }
  function game(value) {
    const label = "state.game";
    object(value, label);
    const players = array(field(value, "players", label), label + ".players").map(function(item, index) {
      return player(item, label + ".players[" + index + "]");
    });
    if (players.length !== 6) fail(label + ".players must contain six seats");
    const current = field(value, "currentPlayer", label);
    const deck = cards(field(value, "deck", label), label + ".deck");
    const nextCard = integer(field(value, "nextCard", label), label + ".nextCard", 0);
    const rngSeed = integer(field(value, "rngSeed", label), label + ".rngSeed", 0);
    if (deck.length !== 0 || nextCard !== 0 || rngSeed !== 0) fail(label + " exposed private deck or RNG state");
    const board = cards(field(value, "board", label), label + ".board");
    if (board.length > 5) fail(label + ".board has too many cards");
    return {
      players: players,
      dealer: seat(field(value, "dealer", label), label + ".dealer"),
      smallBlind: seat(field(value, "smallBlind", label), label + ".smallBlind"),
      bigBlind: seat(field(value, "bigBlind", label), label + ".bigBlind"),
      currentPlayer: current === null ? -1 : seat(current, label + ".currentPlayer"),
      currentBet: integer(field(value, "currentBet", label), label + ".currentBet", 0),
      minRaise: integer(field(value, "minRaise", label), label + ".minRaise", 0),
      street: street(field(value, "street", label), label + ".street"),
      board: board,
      deck: deck,
      nextCard: nextCard,
      handNumber: integer(field(value, "handNumber", label), label + ".handNumber", 0),
      handOver: boolean(field(value, "handOver", label), label + ".handOver"),
      history: array(field(value, "history", label), label + ".history").map(function(item, index) {
        return string(item, label + ".history[" + index + "]", 180);
      }),
      result: string(field(value, "result", label), label + ".result", 512),
      rngSeed: rngSeed,
    };
  }
  function probabilities(value, label) {
    object(value, label);
    const result = Object.entries(value).map(function(entry) {
      const action = entry[0];
      const number = entry[1];
      if (action.length === 0 || action.length > 16) fail(label + " has an invalid action name");
      if (typeof number !== "number" || !Number.isFinite(number) || number < 0 || number > 1) fail(label + "." + action + " must be a probability");
      return {action: action, value: number};
    });
    if (result.length === 0) fail(label + " must not be empty");
    return result;
  }
  function confidence(value, label) {
    if (typeof value !== "number" || !Number.isFinite(value) || value < 0 || value > 1) fail(label + " must be a probability");
    return value;
  }
  function model(value, label) {
    const result = string(value, label, 128);
    if (result.length === 0) fail(label + " must not be empty");
    return result;
  }
  let socket;
  let closed = false;
  let disconnectReported = false;
  function disconnected(message) {
    if (!disconnectReported) {
      disconnectReported = true;
      onDisconnected(message);
    }
  }
  try {
    const protocol = window.location.protocol === "https:" ? "wss://" : "ws://";
    socket = new WebSocket(protocol + window.location.host + "/api/poker/stream");
  } catch (_) {
    disconnected("Unable to connect to the table stream. Reload to retry.");
    return {sendJson: function(_) {}, close: function() { closed = true; }};
  }
  socket.onopen = function() { if (!closed) onOpen(); };
  socket.onmessage = function(event) {
    if (closed || typeof event.data !== "string") return;
    try {
      const message = JSON.parse(event.data);
      object(message, "stream message");
      const version = integer(field(message, "version", "stream message"), "stream message.version", 1);
      if (version !== 1) fail("Unsupported poker stream protocol version");
      const type = string(field(message, "type", "stream message"), "stream message.type", 16);
      if (type === "state") {
        onState(game(field(message, "game", "stream message")));
      } else if (type === "status") {
        const mode = string(field(message, "opponentMode", "status"), "status.opponentMode", 8);
        if (mode !== "jev" && mode !== "local") fail("status.opponentMode is unknown");
        onStatus({
          available: boolean(field(message, "available", "status"), "status.available"),
          model: model(field(message, "model", "status"), "status.model"),
          opponentMode: mode,
        });
      } else if (type === "thinking") {
        street(field(message, "street", "thinking"), "thinking.street");
        model(field(message, "model", "thinking"), "thinking.model");
        integer(field(message, "handNumber", "thinking"), "thinking.handNumber", 1);
        onThinking(seat(field(message, "seat", "thinking"), "thinking.seat"));
      } else if (type === "jevDecision") {
        const decision = object(field(message, "decision", "jevDecision"), "jevDecision.decision");
        if (field(decision, "source", "jevDecision.decision") !== "model") fail("jevDecision.decision.source must be model");
        onDecision({
          seat: seat(field(message, "seat", "jevDecision"), "jevDecision.seat"),
          handNumber: integer(field(message, "handNumber", "jevDecision"), "jevDecision.handNumber", 1),
          street: street(field(message, "street", "jevDecision"), "jevDecision.street"),
          actionLabel: string(field(message, "actionLabel", "jevDecision"), "jevDecision.actionLabel", 80),
          model: model(field(decision, "model", "jevDecision.decision"), "jevDecision.decision.model"),
          confidence: confidence(field(decision, "confidence", "jevDecision.decision"), "jevDecision.decision.confidence"),
          probabilities: probabilities(field(decision, "probabilities", "jevDecision.decision"), "jevDecision.decision.probabilities"),
        });
      } else if (type === "error") {
        onServerError(string(field(message, "message", "error"), "error.message", 512));
      } else {
        fail("Unknown poker stream message type: " + type);
      }
    } catch (error) {
      onServerError("Table stream parse error: " + (error && error.message ? error.message : "invalid message"));
    }
  };
  socket.onerror = function() { disconnected("Table stream error. Reload to reconnect."); };
  socket.onclose = function() {
    closed = true;
    disconnected("Disconnected from the table. Reload to reconnect.");
  };
  return {
    sendJson: function(text) {
      if (!closed && socket.readyState === WebSocket.OPEN) socket.send(text);
    },
    close: function() {
      closed = true;
      disconnectReported = true;
      try { socket.close(); } catch (_) {}
    },
  };
}
|}];

[@native_only]
let rawConnect = (_onOpen, _onState, _onStatus, _onThinking, _onDecision, _onServerError, _onDisconnected) => {
  {sendJson: _ => (), close: () => ()};
};

let suitOfString =
  fun
  | "diamonds" => Diamonds
  | "hearts" => Hearts
  | "spades" => Spades
  | _ => Clubs;

let streetOfString =
  fun
  | "preflop" => Preflop
  | "flop" => Flop
  | "turn" => Turn
  | "river" => River
  | "showdown" => Showdown
  | _ => Lobby;

let cardOfRaw = (card: rawCard): card => {rank: card.rank, suit: suitOfString(card.suit)};

let playerOfRaw = (player: rawPlayer): player => {
  let visibleCards = player.holeCards->Js.Array.map(~f=cardOfRaw);
  /* Empty hidden holeCards plus a count represents card backs only. These
     generic local placeholders are never shown face-up and contain no server
     card information. */
  let holeCards =
    if (Array.length(visibleCards) == 0 && player.holeCardCount > 0) {
      Array.init(player.holeCardCount, _ => ({rank: 2, suit: Clubs} : card));
    } else {
      visibleCards;
    };
  {
    id: player.id,
    name: player.name,
    chips: player.chips,
    holeCards,
    folded: player.folded,
    allIn: player.allIn,
    busted: player.busted,
    committed: player.committed,
    streetCommitted: player.streetCommitted,
    hasActed: player.hasActed,
    canRaise: player.canRaise,
    lastAction: player.lastAction,
  };
};

let gameOfRaw = (game: rawGame): game => {
  players: game.players->Js.Array.map(~f=playerOfRaw),
  dealer: game.dealer,
  smallBlind: game.smallBlind,
  bigBlind: game.bigBlind,
  currentPlayer: game.currentPlayer < 0 ? None : Some(game.currentPlayer),
  currentBet: game.currentBet,
  minRaise: game.minRaise,
  street: streetOfString(game.street),
  board: game.board->Js.Array.map(~f=cardOfRaw),
  deck: [||],
  nextCard: 0,
  handNumber: game.handNumber,
  handOver: game.handOver,
  history: game.history,
  result: game.result,
  rngSeed: 0,
};

let decisionOfRaw = (event: decisionEvent): jevDecision => {
  seat: event.seat,
  handNumber: event.handNumber,
  street: streetOfString(event.street),
  actionLabel: event.actionLabel,
  decision: {
    model: event.model,
    confidence: event.confidence,
    probabilities: event.probabilities,
  },
};

let connect = (~onOpen, ~onState, ~onStatus, ~onThinking, ~onDecision, ~onServerError, ~onDisconnected) =>
  rawConnect(
    onOpen,
    raw => onState(gameOfRaw(raw)),
    onStatus,
    onThinking,
    raw => onDecision(decisionOfRaw(raw)),
    onServerError,
    onDisconnected,
  );

let jsonObject = fields => {
  let dict = Js.Dict.empty();
  fields->Js.Array.forEach(~f=((key, value)) => Js.Dict.set(dict, key, value));
  Js.Json.object_(dict);
};

let intent = (kind, fields) =>
  jsonObject(
    Js.Array.concat(
      ~other=fields,
      [|("version", Js.Json.number(1.0)), ("type", Js.Json.string(kind))|],
    ),
  )->Js.Json.stringify;

let deal = connection => connection.sendJson(intent("deal", [||]));
let restart = connection => connection.sendJson(intent("restart", [||]));
let selectMode = (connection, mode) =>
  connection.sendJson(intent("mode", [|("mode", Js.Json.string(mode))|]));

let act = (connection, action) => {
  let (kind, amount) =
    switch (action) {
    | Fold => ("fold", None)
    | Check => ("check", None)
    | Call => ("call", None)
    | Bet(amount) => ("bet", Some(amount))
    | RaiseTo(amount) => ("raise", Some(amount))
    | AllIn => ("all_in", None)
    };
  let fields =
    switch (amount) {
    | Some(value) => [|("kind", Js.Json.string(kind)), ("amount", Js.Json.number(float_of_int(value)))|]
    | None => [|("kind", Js.Json.string(kind))|]
    };
  connection.sendJson(intent("action", [|("action", jsonObject(fields))|]));
};

let streetName = street =>
  switch (street) {
  | Preflop => "preflop"
  | Flop => "flop"
  | Turn => "turn"
  | River => "river"
  | Lobby => "lobby"
  | Showdown => "showdown"
  };
