open PokerTypes;

let initials = name => String.sub(name, 0, min(1, String.length(name)));

[@react.component]
let make = (~game: game, ~index: int) => {
  let player = game.players[index];
  let active = game.currentPlayer == Some(index);
  let showdown = game.handOver && game.street == Showdown;
  let reveal = index == PokerEngine.humanIndex || (showdown && !player.folded && !player.busted);
  let hasCards = Array.length(player.holeCards) > 0;
  let classes =
    "seat seat-"
    ++ string_of_int(index)
    ++ (active ? " seat-active" : "")
    ++ (player.folded ? " seat-folded" : "")
    ++ (player.busted ? " seat-busted" : "");
  let label =
    player.name
    ++ ", "
    ++ string_of_int(player.chips)
    ++ " chips"
    ++ (game.handNumber > 0 && game.dealer == index && !player.busted ? ", dealer" : "")
    ++ (game.handNumber > 0 && game.smallBlind == index && !player.busted ? ", small blind" : "")
    ++ (game.handNumber > 0 && game.bigBlind == index && !player.busted ? ", big blind" : "")
    ++ (active ? ", active turn" : "")
    ++ (player.busted ? ", busted" : "");

  <div className=classes role="group" ariaLabel=label>
    <div className="avatar" ariaHidden={true}>
      <span> {React.string(initials(player.name))} </span>
    </div>
    {game.handNumber > 0 && game.dealer == index && !player.busted
       ? <span className="dealer-button" title="Dealer" ariaLabel="Dealer"> {React.string("D")} </span>
       : React.null}
    {game.handNumber > 0 && game.smallBlind == index && !player.busted
       ? <span className="blind-badge"> {React.string("SB")} </span>
       : React.null}
    {game.handNumber > 0 && game.bigBlind == index && !player.busted
       ? <span className="blind-badge blind-badge-bb"> {React.string("BB")} </span>
       : React.null}
    <div className="seat-cards">
      {hasCards
         ? Array.init(Array.length(player.holeCards), cardIndex =>
             <PokerCard
               key={string_of_int(game.handNumber) ++ "-" ++ string_of_int(cardIndex)}
               card={Some(player.holeCards[cardIndex])}
               hidden={!reveal}
               compact=true
             />
           )
           ->React.array
         : React.null}
    </div>
    <div className="seat-panel">
      <strong> {React.string(player.name)} </strong>
      <span className="stack"> {React.string(string_of_int(player.chips) ++ " chips")} </span>
      {player.lastAction == ""
         ? React.null
         : <span
               className="last-action"
               key={string_of_int(game.handNumber) ++ PokerEngine.streetName(game.street) ++ player.lastAction}>
               {React.string(player.lastAction)}
             </span>}
    </div>
    {player.streetCommitted > 0 && !game.handOver
       ? <span className="seat-bet"> {React.string("Bet " ++ string_of_int(player.streetCommitted))} </span>
       : React.null}
  </div>;
};
