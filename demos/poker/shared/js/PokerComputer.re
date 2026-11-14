open PokerTypes;

let maxHoleRank = (player: player) =>
  player.holeCards->Js.Array.reduce(~f=(highest, card) => max(highest, card.rank), ~init=0);

let strength = (game: game, player: player) => {
  if (Array.length(player.holeCards) != 2) {
    0;
  } else {
    let first = player.holeCards[0];
    let second = player.holeCards[1];
    let paired = first.rank == second.rank;
    let suited = first.suit == second.suit;
    let gap = abs(first.rank - second.rank);
    let base =
      if (paired) {
        52 + first.rank * 2;
      } else {
        maxHoleRank(player) * 2 + min(first.rank, second.rank) + (suited ? 7 : 0) + (gap <= 2 ? 5 : 0);
      };
    let boardPairBonus =
      game.board->Js.Array.reduce(
        ~f=(bonus, card) => bonus + (card.rank == first.rank || card.rank == second.rank ? 18 : 0),
        ~init=0,
      );
    switch (PokerHand.bestFor(player, game.board)) {
    | Some(value) => max(base + boardPairBonus, 35 + value.category * 22)
    | None => base + boardPairBonus
    };
  };
};

let clamp = (low, high, value) => max(low, min(high, value));

/* This selector is deliberately deterministic and local. A future decision
   adapter can replace this function while continuing to return only actions
   admitted by PokerEngine.legalOptions. */
let chooseAction = (game: game, index: int): action => {
  let player = game.players[index];
  let options = PokerEngine.legalOptions(game, index);
  let score = strength(game, player);
  let toCall = max(0, game.currentBet - player.streetCommitted);
  let pressure = toCall * 100 / max(1, PokerEngine.potTotal(game) + toCall);
  let personality = index mod 3;
  let threshold = 40 + personality * 5;
  let bluff = (game.handNumber + index + PokerEngine.streetNumber(game.street)) mod 7 == 0;

  if (options.canAllIn && score >= 88 && player.chips <= PokerEngine.bigBlindAmount * 4) {
    AllIn;
  } else if (toCall > 0) {
    if (options.canFold && score < threshold && (pressure > 24 || toCall > PokerEngine.bigBlindAmount * 2)) {
      Fold;
    } else if (options.canRaise && score >= 78 + personality * 3) {
      let target = clamp(options.minRaiseTo, options.maxRaiseTo, game.currentBet + max(game.minRaise, PokerEngine.potTotal(game) / 2));
      RaiseTo(target);
    } else if (options.canCall) {
      Call;
    } else if (options.canAllIn && player.chips <= toCall) {
      AllIn;
    } else if (options.canFold) {
      Fold;
    } else {
      Check;
    };
  } else if (options.canBet && (score >= 60 + personality * 4 || bluff)) {
    let target = clamp(options.minBet, options.maxBet, max(PokerEngine.bigBlindAmount, PokerEngine.potTotal(game) * (2 + personality) / 4));
    Bet(target);
  } else if (options.canCheck) {
    Check;
  } else if (options.canCall) {
    Call;
  } else if (options.canFold) {
    Fold;
  } else {
    AllIn;
  };
};
