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

[@melange_only]
let rangeStep = 1.0;

[@native_only]
let rangeStep = "1";

let clamp = (low, high, value) => max(low, min(high, value));

type opponentMode =
  | JevMode
  | LocalMode;

type jevStatus =
  | Checking
  | Unavailable
  | Available(string);

type jevLogEntry = {
  id: int,
  seat: int,
  handNumber: int,
  street: street,
  actionLabel: string,
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

let concreteActionLabel = (current: game, index, action) => {
  let player = current.players[index];
  switch (action) {
  | Fold => "Fold"
  | Check => "Check"
  | Call =>
    let toCall = max(0, current.currentBet - player.streetCommitted);
    let amount = min(toCall, player.chips);
    (amount < toCall ? "All-in " : "Call ") ++ string_of_int(amount);
  | Bet(target) => "Bet " ++ string_of_int(target)
  | RaiseTo(target) => "Raise to " ++ string_of_int(target)
  | AllIn => "All-in " ++ string_of_int(player.chips)
  };
};

let percent = value => string_of_int(int_of_float(value *. 100.0 +. 0.5));

[@react.component]
let make = () => {
  let (game, setGame) = React.useState(() => PokerEngine.createGame());
  let (betTarget, setBetTarget) = React.useState(() => PokerEngine.bigBlindAmount);
  let (message, setMessage) = React.useState(() => "");
  let (opponentMode, setOpponentMode) = React.useState(() => JevMode);
  let (jevStatus, setJevStatus) = React.useState(() => Checking);
  let (jevLog, setJevLog) = React.useState(() => [||]);
  let (soundEnabled, setSoundEnabled) = React.useState(() => false);
  let gameRef = React.useRef(game);
  let jevLogId = React.useRef(0);
  let soundEnabledRef = React.useRef(false);
  gameRef.current = game;

  let playSound = cue =>
    if (soundEnabledRef.current) {
      PokerSound.play(cue);
    };
  let playDealSounds = () => {
    playSound(PokerSound.Shuffle);
    playSound(PokerSound.Deal);
  };
  let playTransitionSounds = (current: game, next: game, action) => {
    switch (action) {
    | Fold => playSound(PokerSound.Fold)
    | Call
    | Bet(_)
    | RaiseTo(_)
    | AllIn => playSound(PokerSound.Chips)
    | Check => ()
    };
    if (Array.length(next.board) > Array.length(current.board)) {
      playSound(PokerSound.Deal);
    };
    if (next.handOver && !current.handOver) {
      playSound(PokerSound.Win);
    };
  };
  let toggleSound = () => {
    let nextEnabled = !soundEnabledRef.current;
    soundEnabledRef.current = nextEnabled;
    PokerSound.setEnabled(nextEnabled);
    setSoundEnabled(_ => nextEnabled);
  };
  let logJevDecision = (seat, handNumber, street, actionLabel, decision) => {
    let id = jevLogId.current + 1;
    jevLogId.current = id;
    let entry = {id, seat, handNumber, street, actionLabel, decision};
    setJevLog(currentEntries => {
      let entries = Js.Array.concat(~other=currentEntries, [|entry|]);
      Js.Array.slice(~start=0, ~end_=min(8, Array.length(entries)), entries);
    });
  };

  let options = PokerEngine.legalOptions(game, PokerEngine.humanIndex);
  let sizingMin = game.currentBet == 0 ? options.minBet : options.minRaiseTo;
  let sizingMax = game.currentBet == 0 ? options.maxBet : options.maxRaiseTo;
  let canSize = (options.canBet || options.canRaise) && sizingMax >= sizingMin;
  let sizingTarget = canSize ? clamp(sizingMin, sizingMax, betTarget) : sizingMin;
  let pot = PokerEngine.potTotal(game);

  React.useEffect0(() => {
    let cancel =
      PokerJevClient.getStatus(
        (status: PokerJevClient.status) => setJevStatus(_ => status.available ? Available(status.model) : Unavailable),
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
              playTransitionSounds(current, next, action);
              setGame(_ => next);
              if (announce) {
                setMessage(_ => "Jev fallback | " ++ current.players[index].name ++ " used local rules");
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
                      logJevDecision(index, handNumber, current.street, concreteActionLabel(current, index, action), decision);
                      playTransitionSounds(current, next, action);
                      setGame(_ => next);
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

  React.useEffect1(
    () => {
      if (
        game.handOver
        && game.players[PokerEngine.humanIndex].chips > 0
        && PokerEngine.fundedCount(game) >= 2
      ) {
        let handNumber = game.handNumber;
        let timer =
          Js.Global.setTimeout(
            ~f=() => {
              let current = gameRef.current;
              if (
                current.handOver
                && current.handNumber == handNumber
                && current.players[PokerEngine.humanIndex].chips > 0
                && PokerEngine.fundedCount(current) >= 2
              ) {
                switch (PokerEngine.startHand(current)) {
                | Ok(next) =>
                  gameRef.current = next;
                  setMessage(_ => "");
                  playDealSounds();
                  setGame(_ => next);
                | Error(error) => setMessage(_ => error)
                };
              };
            },
            2800,
          );
        Some(() => Js.Global.clearTimeout(timer));
      } else {
        None;
      };
    },
    [|game|],
  );

  let deal = () => {
    let current = gameRef.current;
    switch (PokerEngine.startHand(current)) {
    | Ok(next) =>
      setMessage(_ => "");
      gameRef.current = next;
      playDealSounds();
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
  };

  let restart = () => {
    let current = gameRef.current;
    let next = PokerEngine.restart(~seed=current.rngSeed + 97, ());
    setMessage(_ => "");
    setJevLog(_ => [||]);
    jevLogId.current = 0;
    setBetTarget(_ => PokerEngine.bigBlindAmount);
    gameRef.current = next;
    setGame(_ => next);
  };

  let act = action => {
    let current = gameRef.current;
    switch (PokerEngine.applyAction(current, PokerEngine.humanIndex, action)) {
    | Ok(next) =>
      setMessage(_ => "");
      gameRef.current = next;
      playTransitionSounds(current, next, action);
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
  };

  let selectMode = mode => {
    setOpponentMode(_ => mode);
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
        toCall > 0 ? "Your turn | " ++ string_of_int(toCall) ++ " to call" : "Your turn | check or bet";
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
    | (JevMode, Checking) => "Checking Jev availability..."
    | (JevMode, Unavailable) => "Jev unavailable | local fallback"
    | (JevMode, Available(_)) =>
      switch (game.currentPlayer) {
      | Some(index) when index != PokerEngine.humanIndex && !game.handOver =>
        "Jev thinking | " ++ game.players[index].name
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
  let canDealFirst =
    game.handNumber == 0
    && game.players[PokerEngine.humanIndex].chips > 0
    && PokerEngine.fundedCount(game) >= 2;
  let canDealNext =
    game.handOver
    && game.players[PokerEngine.humanIndex].chips > 0
    && PokerEngine.fundedCount(game) >= 2;
  let felted = game.handOver && game.players[PokerEngine.humanIndex].chips <= 0;

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
        <button
          className="sound-toggle"
          type_="button"
          ariaPressed={string_of_bool(soundEnabled)}
          onClick={_ => toggleSound()}>
          {React.string(soundEnabled ? "Sound on" : "Sound off")}
        </button>
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
                         key={string_of_int(game.handNumber) ++ PokerEngine.streetName(game.street) ++ string_of_int(index)}
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

            {game.handNumber == 0
               ? <div className="hand-flow-actions">
                   <button className="primary-action" type_="button" onClick={_ => deal()} disabled={!canDealFirst}>
                     {React.string("Deal first hand")}
                   </button>
                   <button className="secondary-action" type_="button" onClick={_ => restart()}>
                     {React.string("Restart table")}
                   </button>
                 </div>
               : game.handOver
               ? <div className="hand-flow-actions">
                   <span className="next-hand-status" role="status">
                     {React.string(canDealNext ? "Next hand deals automatically in a moment..." : "Restart the table to play again.")}
                   </span>
                   <button className="secondary-action" type_="button" onClick={_ => restart()}>
                     {React.string("Restart table")}
                   </button>
                 </div>
               : <>
                   {canSize
                      ? <div className="bet-sizing">
                          <div className="sizing-heading">
                            <label htmlFor="bet-size"> {React.string(sizingLabel ++ " " ++ string_of_int(sizingTarget))} </label>
                            <span> {React.string(string_of_int(sizingMin) ++ " - " ++ string_of_int(sizingMax))} </span>
                          </div>
                          <input
                            id="bet-size"
                            ariaLabel="Bet or raise target"
                            type_="range"
                            min={string_of_int(sizingMin)}
                            max={string_of_int(sizingMax)}
                            step={rangeStep}
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
                  ariaPressed={string_of_bool(opponentMode == JevMode)}
                  disabled={!jevAvailable}
                  onClick={_ => selectMode(JevMode)}>
                  {React.string("Jev")}
                </button>
                <button
                  type_="button"
                  className={opponentMode == LocalMode ? "opponent-mode-active" : ""}
                  ariaPressed={string_of_bool(opponentMode == LocalMode)}
                  onClick={_ => selectMode(LocalMode)}>
                  {React.string("Local")}
                </button>
              </div>
            </div>
            <span className="opponent-state" role="status"> {React.string(opponentState)} </span>
            {opponentMode == JevMode && configuredModel != ""
               ? <span className="configured-model"> {React.string("Configured: " ++ configuredModel)} </span>
               : React.null}
          </section>
          <section className="rail-card jev-log-card" ariaLabelledby="jev-log-title">
            <div className="rail-heading">
              <h2 id="jev-log-title"> {React.string("Jev decision log")} </h2>
              <span> {React.string("Latest first")} </span>
            </div>
            <p className="jev-log-intro"> {React.string("Actual model responses only; local fallback decisions are not logged.")} </p>
            {Array.length(jevLog) == 0
               ? <p className="jev-log-empty"> {React.string("No Jev decisions yet.")} </p>
               : <ol>
                   {Array.init(Array.length(jevLog), index => {
                      let entry = jevLog[index];
                      let decision = entry.decision;
                      let handLabel =
                        "Hand " ++ string_of_int(entry.handNumber)
                        ++ " | " ++ PokerJevClient.streetName(entry.street);
                      <li key={string_of_int(entry.id)}>
                        <article ariaLabel={game.players[entry.seat].name ++ ", " ++ handLabel ++ ", " ++ entry.actionLabel}>
                          <header className="jev-entry-heading">
                            <strong>
                              {React.string(
                                 game.players[entry.seat].name
                                 ++ " (seat " ++ string_of_int(entry.seat + 1) ++ ") | "
                                 ++ entry.actionLabel
                               )}
                            </strong>
                            <span> {React.string(handLabel)} </span>
                          </header>
                          <dl className="jev-metadata">
                            <div>
                              <dt> {React.string("Model")} </dt>
                              <dd> {React.string(decision.model)} </dd>
                            </div>
                            <div>
                              <dt> {React.string("Confidence")} </dt>
                              <dd> {React.string(percent(decision.confidence) ++ "%")} </dd>
                            </div>
                          </dl>
                          <ul className="jev-probabilities" ariaLabel="Action probabilities">
                            {Array.init(Array.length(decision.probabilities), probabilityIndex => {
                               let probability = decision.probabilities[probabilityIndex];
                               let probabilityPercent = percent(probability.value);
                               <li key={probability.action}>
                                 <span className="jev-probability-action"> {React.string(probability.action)} </span>
                                 <progress
                                   max="100"
                                   value=probabilityPercent
                                   ariaLabel={probability.action ++ " probability " ++ probabilityPercent ++ "%"}
                                 />
                                 <span className="jev-probability-value"> {React.string(probabilityPercent ++ "%")} </span>
                               </li>;
                             })->React.array}
                          </ul>
                        </article>
                      </li>;
                    })->React.array}
                 </ol>}
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
            {React.string("Play chips only. Jev receives only the acting opponent's two cards and public table state. Local rules remain available; no accounts, wagers, or tracking.")}
          </p>
        </aside>
      </section>
    </main>
    <footer className="room-footer">
      <span> {React.string("UNIVERSAL REASON REACT")} </span>
      <span> {React.string("SSR lobby | browser-dealt hands | standard 52-card deck")} </span>
    </footer>
    {felted
       ? <div className="felted-overlay" role="dialog" ariaModal={true} ariaLabelledby="felted-title" ariaDescribedby="felted-description">
           <div className="felted-dialog">
             <span className="eyebrow"> {React.string("HAND COMPLETE")} </span>
             <h2 id="felted-title"> {React.string("You are felted")} </h2>
             <p id="felted-description"> {React.string("Your stack is empty. Restart the table for a fresh set of play chips.")} </p>
             <button className="primary-action" type_="button" autoFocus={true} onClick={_ => restart()}>
               {React.string("Restart table")}
             </button>
           </div>
         </div>
       : React.null}
  </div>;
};
