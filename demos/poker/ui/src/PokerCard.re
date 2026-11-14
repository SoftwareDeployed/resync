open PokerTypes;

[@react.component]
let make = (~card: option(card), ~hidden=false, ~compact=false) => {
  let sizeClass = compact ? " playing-card-compact" : "";
  switch (card) {
  | None =>
    <div className={"playing-card card-placeholder" ++ sizeClass} ariaHidden="true" />
  | Some(_card) when hidden =>
    <div className={"playing-card card-back" ++ sizeClass} role="img" ariaLabel="Face-down card">
      <span className="card-back-mark" ariaHidden="true"> {React.string("R")} </span>
    </div>
  | Some(card) =>
    let red = card.suit == Hearts || card.suit == Diamonds;
    let suitClass = "suit-" ++ PokerHand.suitLetter(card.suit);
    <div
      className={"playing-card" ++ sizeClass ++ (red ? " card-red" : " card-black")}
      role="img"
      ariaLabel={PokerHand.cardName(card)}>
      <span className="card-rank"> {React.string(PokerHand.rankLabel(card.rank))} </span>
      <span className={"card-suit " ++ suitClass} ariaHidden="true">
        {React.string(PokerHand.suitLetter(card.suit))}
      </span>
      <span className="sr-only"> {React.string(PokerHand.cardName(card))} </span>
    </div>
  };
};
