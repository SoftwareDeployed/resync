open PokerTypes;

[@melange_only]
module Style = {
  [%%raw "import \"./style.css\""];
};

[@native_only]
module Style = {
  let _css = ();
};

[@melange_only]
let inputValue = event => React.Event.Form.target(event)##value;

[@native_only]
let inputValue = _event => "";

let clamp = (low, high, value) => max(low, min(high, value));

type opponentMode =
  | JevMode
  | LocalMode;

type jevStatus =
  | Checking
  | Unavailable
  | Available(string);

type lastJevDecision = {
  seat: int,
  decision: PokerJevClient.decision,
};

let decisionIsCurrent = (game: game, handNumber, index, body) =>
  !game.handOver
  && game.handNumber == handNumber
  && game.currentPlayer == Some(index)
  && index != PokerEngine.humanIndex
  && PokerJevClient.snapshot(game, index) == body;

let actionOfDecision = (decision: PokerJevClient.decision) =>
  switch (decision.kind) {
  | "fold" => Some(Fold)
  | "check" => Some(Check)
  | "call" => Some(Call)
  | "bet" => Some(Bet(decision.amount))
  | "raise" => Some(RaiseTo(decision.amount))
  | "all_in" => Some(AllIn)
  | _ => None
  };

let percent = value => string_of_int(int_of_float(value *. 100.0 +. 0.5));

let decisionLabel = (decision: PokerJevClient.decision) =>
  switch (decision.kind) {
  | "fold" => "fold"
  | "check" => "check"
  | "call" => "call"
  | "bet" => "bet " ++ string_of_int(decision.amount)
  | "raise" => "raise to " ++ string_of_int(decision.amount)
  | "all_in" => "all-in"
  | _ => "decision"
  };

let probabilitySummary = probabilities =>
  probabilities
  ->Js.Array.map(~f=(probability: PokerJevClient.probability) =>
      probability.action ++ " " ++ percent(probability.value) ++ "%"
    )
  ->Js.Array.reduce(~f=(text, item) => text == "" ? item : text ++ " · " ++ item, ~init="");

@react.component
let make = () => {
  let (game, setGame) = React.useState(() => PokerEngine.createGame());
  let (betTarget, setBetTarget) = React.useState(() => PokerEngine.bigBlindAmount);
  let (message, setMessage) = React.useState(() => "");
  let (opponentMode, setOpponentMode) = React.useState(() => JevMode);
  let (jevStatus, setJevStatus) = React.useState(() => Checking);
  let (lastJev, setLastJev) = React.useState(() => None);
  let gameRef = React.useRef(game);
  gameRef.current = game;
  let options = PokerEngine.legalOptions(game, PokerEngine.humanIndex);
  let sizingMin = game.currentBet == 0 ? options.minBet : options.minRaiseTo;
  let sizingMax = game.currentBet == 0 ? options.maxBet : options.maxRaiseTo;
  let canSize = (options.canBet || options.canRaise) && sizingMax >= sizingMin;
  let sizingTarget = canSize ? clamp(sizingMin, sizingMax, betTarget) : sizingMin;
  let pot = PokerEngine.potTotal(game);

  React.useEffect0(() => {
    let cancel =
      PokerJevClient.getStatus(
        status => setJevStatus(_ => status.available ? Available(status.model) : Unavailable),
        () => setJevStatus(_ => Unavailable),
      );
    Some(cancel);
  });

  React.useEffect3(
    () => {
      switch (game.currentPlayer) {
      | Some(index) when index != PokerEngine.humanIndex && !game.handOver =>
        let handNumber = game.handNumber;
        let body = PokerJevClient.snapshot(game, index);
        let applyLocal = (~announce: bool) => {
          let current = gameRef.current;
          if (decisionIsCurrent(current, handNumber, index, body)) {
            let action = PokerComputer.chooseAction(current, index);
            switch (PokerEngine.applyAction(current, index, action)) {
            | Ok(next) =>
              gameRef.current = next;
              setGame(_ => next);
              setLastJev(_ => None);
              if (announce) {
                setMessage(_ => "Jev fallback • " ++ current.players[index].name ++ " used local rules");
              };
            | Error(error) => setMessage(_ => error)
            };
          };
        };
        switch (opponentMode, jevStatus) {
        | (JevMode, Available(_)) =>
          let cancel =
            PokerJevClient.postDecision(
              "/api/poker/jev/decision",
              body,
              decision => {
                let current = gameRef.current;
                if (decisionIsCurrent(current, handNumber, index, body)) {
                  switch (actionOfDecision(decision)) {
                  | None => applyLocal(~announce=true)
                  | Some(action) =>
                    switch (PokerEngine.applyAction(current, index, action)) {
                    | Ok(next) =>
                      gameRef.current = next;
                      setGame(_ => next);
                      setLastJev(_ => Some({seat: index, decision}));
                      setMessage(_ => "");
                    | Error(_) => applyLocal(~announce=true)
                    }
                  };
                };
              },
              () => applyLocal(~announce=true),
            );
          Some(cancel);
        | (JevMode, Checking) => None
        | _ =>
          let timer = Js.Global.setTimeout(~f=() => applyLocal(~announce=opponentMode == JevMode), 620);
          Some(() => Js.Global.clearTimeout(timer));
        };
      | _ => None
      };
    },
    (game, opponentMode, jevStatus),
  );

  React.useEffect1(
    () => {
      if (canSize) {
        setBetTarget(_ => sizingMin);
      };
      None;
    },
    [|game|],
  );

  let deal = () => {
    switch (PokerEngine.startHand(game)) {
    | Ok(next) =>
      setMessage(_ => "");
      setLastJev(_ => None);
      gameRef.current = next;
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
  };

  let restart = () => {
    let next = PokerEngine.restart(~seed=game.rngSeed + 97, ());
    setMessage(_ => "");
    setLastJev(_ => None);
    setBetTarget(_ => PokerEngine.bigBlindAmount);
    gameRef.current = next;
    setGame(_ => next);
  };

  let act = action => {
    switch (PokerEngine.applyAction(game, PokerEngine.humanIndex, action)) {
    | Ok(next) =>
      setMessage(_ => "");
      gameRef.current = next;
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
  };

  let selectMode = mode => {
    setOpponentMode(_ => mode);
    setLastJev(_ => None);
    setMessage(_ => "");
  };

  let status =
    if (game.handNumber == 0) {
      "Welcome in. Deal the first hand when you are ready.";
    } else if (game.handOver) {
      game.result;
    } else {
      switch (game.currentPlayer) {
      | Some(index) when index == PokerEngine.humanIndex =>
        let toCall = max(0, game.currentBet - game.players[index].streetCommitted);
        toCall > 0 ? "Your turn • " ++ string_of_int(toCall) ++ " to call" : "Your turn • check or bet";
      | Some(index) =>
        game.players[index].name
        ++ (
          switch (opponentMode, jevStatus) {
          | (JevMode, Available(_)) => " is thinking with Jev"
          | _ => " is considering the action"
          }
        )
      | None => "Completing the board"
      };
    };

  let opponentState =
    switch (opponentMode, jevStatus) {
    | (LocalMode, _) => "Local opponents"
    | (JevMode, Checking) => "Checking Jev availability…"
    | (JevMode, Unavailable) => "Jev unavailable • local fallback"
    | (JevMode, Available(_)) =>
      switch (game.currentPlayer) {
      | Some(index) when index != PokerEngine.humanIndex && !game.handOver =>
        "Jev thinking • " ++ game.players[index].name
      | _ => "Jev opponents ready"
      }
    };

  let configuredModel =
    switch (jevStatus) {
    | Available(model) => model
    | _ => ""
    };
  let jevAvailable = configuredModel != "";

  let sizingAction = game.currentBet == 0 ? Bet(sizingTarget) : RaiseTo(sizingTarget);
  let sizingLabel = game.currentBet == 0 ? "Bet" : "Raise to";
  let potSizedTarget =
    clamp(sizingMin, sizingMax, game.currentBet == 0 ? max(sizingMin, pot) : game.currentBet + max(game.minRaise, pot));
  let halfPotTarget =
    clamp(sizingMin, sizingMax, game.currentBet == 0 ? max(sizingMin, pot / 2) : game.currentBet + max(game.minRaise, pot / 2));
  let canDeal = (game.handNumber == 0 || game.handOver) && game.players[PokerEngine.humanIndex].chips > 0 && PokerEngine.fundedCount(game) >= 2;

  <div className="poker-app">
    <header className="room-header">
      <div className="brand-lockup">
        <span className="brand-mark" ariaHidden={true}> {React.string("R")} </span>
        <div>
          <span className="eyebrow"> {React.string("RESYNC / TABLE 06")} </span>
          <h1> {React.string("River Room")} </h1>
        </div>
      </div>
      <div className="room-meta">
        <span className="live-dot" ariaHidden={true} />
        <span> {React.string("Play chips")} </span>
        <span className="meta-divider" ariaHidden={true} />
        <span> {React.string("10 / 20")} </span>
      </div>
    </header>

    <main>
      <section className="game-layout" ariaLabel="Six-handed Texas Hold'em poker room">
        <div className="table-column">
          <div className="table-toolbar">
            <div>
              <span className="eyebrow"> {React.string("HAND " ++ string_of_int(game.handNumber))} </span>
              <strong> {React.string(PokerEngine.streetName(game.street))} </strong>
            </div>
            <div className="toolbar-chips">
              <span> {React.string("Pot")} </span>
              <strong> {React.string(string_of_int(pot))} </strong>
            </div>
          </div>

          <div className="table-scroll" tabIndex=0 ariaLabel="Poker table; scroll horizontally on small screens">
            <div className="table-stage">
              <div className="table-felt">
                <div className="felt-line" ariaHidden={true} />
                <div className="board-area">
                  <span className="board-label"> {React.string("COMMUNITY CARDS")} </span>
                  <div className="board-cards">
                    {Array.init(5, index =>
                       <PokerCard
                         key={string_of_int(index)}
                         card={index < Array.length(game.board) ? Some(game.board[index]) : None}
                       />
                     )->React.array}
                  </div>
                  <div className="pot-display">
                    <span> {React.string("TOTAL POT")} </span>
                    <strong> {React.string(string_of_int(pot))} </strong>
                  </div>
                </div>
                {Array.init(PokerEngine.playerCount, index => <PokerSeat key={string_of_int(index)} game index />)->React.array}
              </div>
            </div>
          </div>

          <div className="status-bar" role="status" ariaLive="polite" ariaAtomic={true}>
            <span className={game.currentPlayer == Some(PokerEngine.humanIndex) ? "status-pip status-pip-active" : "status-pip"} ariaHidden={true} />
            <span> {React.string(message == "" ? status : message)} </span>
          </div>

          <section className="action-dock" ariaLabel="Poker actions">
            <div className="action-topline">
              <div>
                <span className="eyebrow"> {React.string("YOUR DECISION")} </span>
                <strong> {React.string(game.handOver ? "Hand complete" : game.currentPlayer == Some(0) ? "Choose a legal action" : "Opponents are playing")} </strong>
              </div>
              <span className="your-stack"> {React.string("Your stack " ++ string_of_int(game.players[0].chips))} </span>
            </div>

            {game.handOver || game.handNumber == 0
               ? <div className="hand-flow-actions">
                   <button className="primary-action" type_="button" onClick={_ => deal()} disabled={!canDeal}>
                     {React.string(game.handNumber == 0 ? "Deal first hand" : "Deal next hand")}
                   </button>
                   <button className="secondary-action" type_="button" onClick={_ => restart()}>
                     {React.string("Restart table")}
                   </button>
                 </div>
               : <>
                   {canSize
                      ? <div className="bet-sizing">
                          <div className="sizing-heading">
                            <label htmlFor="bet-size"> {React.string(sizingLabel ++ " " ++ string_of_int(sizingTarget))} </label>
                            <span> {React.string(string_of_int(sizingMin) ++ " – " ++ string_of_int(sizingMax))} </span>
                          </div>
                          <input
                            id="bet-size"
                            ariaLabel="Bet or raise target"
                            type_="range"
                            min={string_of_int(sizingMin)}
                            max={string_of_int(sizingMax)}
                            step="1"
                            value={string_of_int(sizingTarget)}
                            onChange={event =>
                              switch (int_of_string_opt(inputValue(event))) {
                              | Some(value) => setBetTarget(_ => value)
                              | None => ()
                              }}
                          />
                          <div className="size-presets" ariaLabel="Bet size presets">
                            <button type_="button" onClick={_ => setBetTarget(_ => sizingMin)}> {React.string("Min")} </button>
                            <button type_="button" onClick={_ => setBetTarget(_ => halfPotTarget)}> {React.string("1/2 pot")} </button>
                            <button type_="button" onClick={_ => setBetTarget(_ => potSizedTarget)}> {React.string("Pot")} </button>
                            <button type_="button" onClick={_ => setBetTarget(_ => sizingMax)}> {React.string("Max")} </button>
                          </div>
                        </div>
                      : React.null}
                   <div className="action-buttons">
                     <button className="fold-action" type_="button" disabled={!options.canFold} onClick={_ => act(Fold)}>
                       {React.string("Fold")}
                     </button>
                     <button type_="button" disabled={!options.canCheck} onClick={_ => act(Check)}>
                       {React.string("Check")}
                     </button>
                     <button type_="button" disabled={!options.canCall} onClick={_ => act(Call)}>
                       {React.string(options.callAmount > 0 ? "Call " ++ string_of_int(options.callAmount) : "Call")}
                     </button>
                     <button className="raise-action" type_="button" disabled={!canSize} onClick={_ => act(sizingAction)}>
                       {React.string(sizingLabel ++ " " ++ string_of_int(sizingTarget))}
                     </button>
                     <button className="allin-action" type_="button" disabled={!options.canAllIn} onClick={_ => act(AllIn)}>
                       {React.string("All-in")}
                     </button>
                   </div>
                 </>}
          </section>
        </div>

        <aside className="room-rail" ariaLabel="Hand information">
          <section className="rail-card opponent-card" ariaLabel="Opponent engine">
            <div className="opponent-heading">
              <span className="eyebrow"> {React.string("OPPONENTS")} </span>
              <div className="opponent-modes" role="group" ariaLabel="Opponent decision engine">
                <button
                  type_="button"
                  className={opponentMode == JevMode ? "opponent-mode-active" : ""}
                  aria-pressed={opponentMode == JevMode}
                  disabled={!jevAvailable}
                  onClick={_ => selectMode(JevMode)}>
                  {React.string("Jev")}
                </button>
                <button
                  type_="button"
                  className={opponentMode == LocalMode ? "opponent-mode-active" : ""}
                  aria-pressed={opponentMode == LocalMode}
                  onClick={_ => selectMode(LocalMode)}>
                  {React.string("Local")}
                </button>
              </div>
            </div>
            <span className="opponent-state" role="status"> {React.string(opponentState)} </span>
            {opponentMode == JevMode && configuredModel != ""
               ? <span className="configured-model"> {React.string("Configured: " ++ configuredModel)} </span>
               : React.null}
            {switch (lastJev) {
             | Some({seat, decision}) =>
               <div className="jev-result">
                 <span className="eyebrow"> {React.string("LAST ACTUAL JEV DECISION")} </span>
                 <strong> {React.string(decision.model)} </strong>
                 <p>
                   {React.string(
                      game.players[seat].name ++ " • " ++ decisionLabel(decision)
                      ++ " • confidence " ++ percent(decision.confidence) ++ "%"
                    )}
                 </p>
                 <p className="jev-probabilities"> {React.string(probabilitySummary(decision.probabilities))} </p>
               </div>
             | None => React.null
             }}
          </section>
          <section className="rail-card result-card">
            <span className="eyebrow"> {React.string("TABLE STATUS")} </span>
            <h2> {React.string(game.handOver ? "Result" : PokerEngine.streetName(game.street))} </h2>
            <p> {React.string(status)} </p>
            <dl className="hand-facts">
              <div> <dt> {React.string("Blinds")} </dt> <dd> {React.string("10 / 20")} </dd> </div>
              <div> <dt> {React.string("Players")} </dt> <dd> {React.string(string_of_int(PokerEngine.fundedCount(game)) ++ " funded")} </dd> </div>
              <div> <dt> {React.string("Chip value")} </dt> <dd> {React.string(string_of_int(PokerEngine.chipTotal(game)))} </dd> </div>
            </dl>
          </section>
          <section className="rail-card history-card">
            <div className="rail-heading">
              <span className="eyebrow"> {React.string("ACTION HISTORY")} </span>
              <span> {React.string("Latest first")} </span>
            </div>
            <ol>
              {Array.init(Array.length(game.history), index =>
                 <li key={string_of_int(index) ++ game.history[index]}> {React.string(game.history[index])} </li>
               )->React.array}
            </ol>
          </section>
          <p className="fair-play-note">
            {React.string("Play chips only. Jev receives only the acting opponent’s two cards and public table state. Local rules remain available; no accounts, wagers, or tracking.")}
          </p>
        </aside>
      </section>
    </main>
    <footer className="room-footer">
      <span> {React.string("UNIVERSAL REASON REACT")} </span>
      <span> {React.string("SSR lobby • browser-dealt hands • standard 52-card deck")} </span>
    </footer>
  </div>;
};
