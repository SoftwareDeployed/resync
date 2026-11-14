open PokerTypes;

module SuitIcon = {
  let path = suit =>
    switch (suit) {
    | Spades => "M32 4C26 15 8 25 8 39c0 9 11 14 20 7l-3 14h14l-3-14c9 7 20 2 20-7C56 25 38 15 32 4Z"
    | Hearts => "M32 56 9 34C-5 19 14 3 27 15l5 5 5-5C50 3 69 19 55 34Z"
    | Diamonds => "M32 3 55 32 32 61 9 32Z"
    | Clubs => "M32 4a12 12 0 0 0-8 21 12 12 0 1 0 3 21l-4 12h18l-4-12a12 12 0 1 0 3-21A12 12 0 0 0 32 4Z"
    };

  [@react.component]
  let make = (~suit) =>
    <svg className="suit-icon" viewBox="0 0 64 64" fill="currentColor" ariaHidden={true}>
      <path d={path(suit)} />
    </svg>;
};

[@react.component]
let make = (~card: option(card), ~hidden=false, ~compact=false) => {
  let sizeClass = compact ? " playing-card-compact" : "";
  switch (card) {
  | None =>
    <div className={"playing-card card-placeholder" ++ sizeClass} ariaHidden={true} />
  | Some(_card) when hidden =>
    <div className={"playing-card card-back card-deal-in" ++ sizeClass} role="img" ariaLabel="Face-down card">
      <span className="card-back-mark" ariaHidden={true}> {React.string("R")} </span>
    </div>
  | Some(card) =>
    let red = card.suit == Hearts || card.suit == Diamonds;
    let suitClass = "suit-" ++ PokerHand.suitLetter(card.suit);
    <div
      className={"playing-card card-deal-in" ++ sizeClass ++ (red ? " card-red" : " card-black")}
      role="img"
      ariaLabel={PokerHand.cardName(card)}>
      <span className="card-rank"> {React.string(PokerHand.rankLabel(card.rank))} </span>
      <span className={"card-suit " ++ suitClass} ariaHidden={true}>
        <SuitIcon suit={card.suit} />
      </span>
      <span className="sr-only"> {React.string(PokerHand.cardName(card))} </span>
    </div>
  };
};
