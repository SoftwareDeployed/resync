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

[@react.component]
let make = () => {
  let (game, setGame) = React.useState(() => PokerEngine.createGame());
  let (betTarget, setBetTarget) = React.useState(() => PokerEngine.bigBlindAmount);
  let (message, setMessage) = React.useState(() => "");
  let options = PokerEngine.legalOptions(game, PokerEngine.humanIndex);
  let sizingMin = game.currentBet == 0 ? options.minBet : options.minRaiseTo;
  let sizingMax = game.currentBet == 0 ? options.maxBet : options.maxRaiseTo;
  let canSize = (options.canBet || options.canRaise) && sizingMax >= sizingMin;
  let sizingTarget = canSize ? clamp(sizingMin, sizingMax, betTarget) : sizingMin;
  let pot = PokerEngine.potTotal(game);

  React.useEffect1(
    () => {
      switch (game.currentPlayer) {
      | Some(index) when index != PokerEngine.humanIndex && !game.handOver =>
        let timer =
          Js.Global.setTimeout(
            ~f=() =>
              setGame(current => {
                switch (current.currentPlayer) {
                | Some(currentIndex) when currentIndex == index && currentIndex != PokerEngine.humanIndex =>
                  let action = PokerComputer.chooseAction(current, currentIndex);
                  switch (PokerEngine.applyAction(current, currentIndex, action)) {
                  | Ok(next) => next
                  | Error(_) => current
                  };
                | _ => current
                }
              }),
            620,
          );
        Some(() => Js.Global.clearTimeout(timer));
      | _ => None
      };
    },
    [|game|],
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
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
  };

  let restart = () => {
    setMessage(_ => "");
    setBetTarget(_ => PokerEngine.bigBlindAmount);
    setGame(_ => PokerEngine.restart(~seed=game.rngSeed + 97, ()));
  };

  let act = action => {
    switch (PokerEngine.applyAction(game, PokerEngine.humanIndex, action)) {
    | Ok(next) =>
      setMessage(_ => "");
      setGame(_ => next);
    | Error(error) => setMessage(_ => error)
    };
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
      | Some(index) => game.players[index].name ++ " is considering the action"
      | None => "Completing the board"
      };
    };

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
        <span> {React.string("Local table")} </span>
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
                            step={1.0}
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
            {React.string("Play chips only. Five deterministic local opponents; no accounts, wagers, tracking, or external card data.")}
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
