open PokerTypes;

let card = (rank, suit): card => {rank, suit};

let getOk = result =>
  switch (result) {
  | Ok(value) => value
  | Error(message) => Alcotest.fail(message)
  };

let evaluatorOrdersWheelAndPairs = () => {
  let wheel =
    PokerHand.evaluate([|
      card(14, Clubs), card(2, Diamonds), card(3, Hearts), card(4, Spades),
      card(5, Clubs), card(13, Diamonds), card(13, Hearts),
    |]);
  let sixHigh =
    PokerHand.evaluate([|
      card(2, Clubs), card(3, Diamonds), card(4, Hearts), card(5, Spades),
      card(6, Clubs), card(13, Diamonds), card(13, Hearts),
    |]);
  switch (wheel, sixHigh) {
  | (Some(wheel), Some(sixHigh)) =>
    Alcotest.check(Alcotest.int, "wheel category", 4, wheel.category);
    Alcotest.check(Alcotest.int, "wheel high card", 5, wheel.ranks[0]);
    Alcotest.check(Alcotest.int, "six-high straight", 6, sixHigh.ranks[0]);
    Alcotest.check(Alcotest.bool, "six-high beats wheel", true, PokerHand.compareValue(sixHigh, wheel) > 0)
  | _ => Alcotest.fail("Expected evaluated straights")
  };
};

let deterministicDealAndConservation = () => {
  let initial = PokerEngine.createGame(~seed=42, ());
  Alcotest.check(Alcotest.int, "SSR lobby has no board", 0, Array.length(initial.board));
  Alcotest.check(Alcotest.bool, "SSR lobby has no hole cards", true,
    initial.players->Js.Array.every(~f=player => Array.length(player.holeCards) == 0));
  let dealt = getOk(PokerEngine.startHand(initial));
  let dealtAgain = getOk(PokerEngine.startHand(initial));
  Alcotest.check(Alcotest.int, "twelve hole cards", 12,
    dealt.players->Js.Array.reduce(~f=(total, player) => total + Array.length(player.holeCards), ~init=0));
  Alcotest.check(Alcotest.bool, "seeded deal is reproducible", true,
    dealt.players[0].holeCards->Js.Array.every(~f=card =>
      card == dealtAgain.players[0].holeCards[0]
      || card == dealtAgain.players[0].holeCards[1]
    ));
  Alcotest.check(Alcotest.int, "blinds conserve chips", 6000, PokerEngine.chipTotal(dealt));

  let rec play = (game, remaining) => {
    Alcotest.check(Alcotest.int, "every transition conserves chips", 6000, PokerEngine.chipTotal(game));
    if (game.handOver) {
      game;
    } else if (remaining <= 0) {
      Alcotest.fail("Hand did not complete within 500 actions");
    } else {
      switch (game.currentPlayer) {
      | None => Alcotest.fail("Incomplete hand has no legal actor")
      | Some(index) =>
        let action = PokerComputer.chooseAction(game, index);
        play(getOk(PokerEngine.applyAction(game, index, action)), remaining - 1);
      };
    };
  };
  let completed = play(dealt, 500);
  Alcotest.check(Alcotest.bool, "hand completes", true, completed.handOver);
  Alcotest.check(Alcotest.int, "showdown or fold payout conserves chips", 6000, PokerEngine.chipTotal(completed));
};

let sidePotSettlement = () => {
  let base = PokerEngine.createGame();
  let players = [|
    {...base.players[0], chips: 950, committed: 50, streetCommitted: 50, allIn: true,
     holeCards: [|card(14, Clubs), card(14, Diamonds)|]},
    {...base.players[1], chips: 900, committed: 100, streetCommitted: 100, allIn: true,
     holeCards: [|card(13, Hearts), card(13, Spades)|]},
    {...base.players[2], chips: 900, committed: 100, streetCommitted: 100, allIn: true,
     holeCards: [|card(12, Hearts), card(12, Spades)|]},
    {...base.players[3], chips: 980, committed: 20, streetCommitted: 20, folded: true},
    {...base.players[4], folded: true, busted: true},
    {...base.players[5], folded: true, busted: true},
  |];
  let game = {
    ...base,
    players,
    board: [|
      card(2, Clubs), card(3, Diamonds), card(4, Hearts), card(8, Spades), card(9, Clubs),
    |],
    street: River,
    handNumber: 1,
    handOver: false,
    dealer: 5,
    currentPlayer: None,
  };
  Alcotest.check(Alcotest.int, "side-pot setup is conserved", 6000, PokerEngine.chipTotal(game));
  let settled = PokerEngine.settleShowdown(game);
  Alcotest.check(Alcotest.int, "aces win 80 plus 90 from the first two layers", 1120, settled.players[0].chips);
  Alcotest.check(Alcotest.int, "kings win the 100-chip final layer", 1000, settled.players[1].chips);
  Alcotest.check(Alcotest.int, "queens do not win the final layer", 900, settled.players[2].chips);
  Alcotest.check(Alcotest.int, "folded contribution moves to the winners", 980, settled.players[3].chips);
  Alcotest.check(Alcotest.int, "all side-pot chips conserved", 6000, PokerEngine.chipTotal(settled));
  Alcotest.check(Alcotest.int, "showdown clears the displayed pot", 0, PokerEngine.potTotal(settled));
  Alcotest.check(Alcotest.bool, "showdown clears every contribution", true,
    settled.players->Js.Array.every(~f=player => player.committed == 0 && player.streetCommitted == 0));
};

let evaluatorHandlesFullHousesAndTies = () => {
  let fullHouse =
    PokerHand.evaluate([|
      card(7, Clubs), card(7, Diamonds), card(7, Hearts), card(9, Spades),
      card(9, Diamonds), card(2, Clubs), card(3, Hearts),
    |]);
  let flush =
    PokerHand.evaluate([|
      card(2, Hearts), card(6, Hearts), card(9, Hearts), card(11, Hearts),
      card(13, Hearts), card(3, Clubs), card(4, Diamonds),
    |]);
  switch (fullHouse, flush) {
  | (Some(fullHouse), Some(flush)) =>
    Alcotest.check(Alcotest.int, "full house category", 6, fullHouse.category);
    Alcotest.check(Alcotest.int, "full house trips", 7, fullHouse.ranks[0]);
    Alcotest.check(Alcotest.int, "full house pair", 9, fullHouse.ranks[1]);
    Alcotest.check(Alcotest.bool, "full house beats flush", true,
      PokerHand.compareValue(fullHouse, flush) > 0)
  | _ => Alcotest.fail("Expected full house and flush")
  };

  let board = [|
    card(10, Clubs), card(11, Diamonds), card(12, Hearts),
    card(13, Spades), card(2, Clubs),
  |];
  let first = PokerHand.evaluate(Js.Array.concat(~other=board, [|
    card(14, Clubs), card(14, Diamonds),
  |]));
  let second = PokerHand.evaluate(Js.Array.concat(~other=board, [|
    card(14, Hearts), card(14, Spades),
  |]));
  switch (first, second) {
  | (Some(first), Some(second)) =>
    Alcotest.check(Alcotest.int, "shared broadway straight", 4, first.category);
    Alcotest.check(Alcotest.bool, "equal seven-card hands tie", true,
      PokerHand.compareValue(first, second) == 0)
  | _ => Alcotest.fail("Expected tied straights")
  };
};

let shuffledDeckHasEveryCardOnce = () => {
  let (first, firstSeed) = PokerEngine.shuffledDeck(42);
  let (second, secondSeed) = PokerEngine.shuffledDeck(42);
  Alcotest.check(Alcotest.int, "52-card deck", 52, Array.length(first));
  Alcotest.check(Alcotest.int, "seed advances deterministically", firstSeed, secondSeed);
  let rec uniqueFrom = index =>
    if (index == Array.length(first)) {
      true;
    } else {
      let rest = Js.Array.slice(~start=index + 1, ~end_=Array.length(first), first);
      !rest->Js.Array.some(~f=card => card == first[index]) && uniqueFrom(index + 1);
    };
  Alcotest.check(Alcotest.bool, "no duplicated cards", true, uniqueFrom(0));
  Alcotest.check(Alcotest.bool, "same seed deals same deck", true, first == second);
};

let shortRaiseGame = () => {
  let base = PokerEngine.createGame();
  let players = Array.init(PokerEngine.playerCount, index => {
    ...base.players[index],
    chips: index == 0 ? 25 : 1000,
    committed: index == 0 ? 0 : 20,
    streetCommitted: index == 0 ? 0 : 20,
    hasActed: false,
    canRaise: true,
  });
  {
    ...base,
    players,
    dealer: 5,
    smallBlind: 0,
    bigBlind: 1,
    currentPlayer: Some(0),
    currentBet: 20,
    minRaise: 20,
    street: Preflop,
    handNumber: 1,
  };
};

let fundedBettingGame = () => {
  let game = shortRaiseGame();
  {
    ...game,
    players: PokerEngine.replacePlayer(
      game.players,
      0,
      {...game.players[0], chips: 1000},
    ),
  };
};

let bettingTransitions = () => {
  let callGame = fundedBettingGame();
  let called = getOk(PokerEngine.applyAction(callGame, 0, Call));
  Alcotest.check(Alcotest.int, "call deducts only amount owed", 980, called.players[0].chips);
  Alcotest.check(Alcotest.int, "call updates street contribution", 20, called.players[0].streetCommitted);
  Alcotest.check(Alcotest.int, "call updates total contribution", 20, called.players[0].committed);
  Alcotest.check(Alcotest.bool, "action advances clockwise", true, called.currentPlayer == Some(1));
  Alcotest.check(Alcotest.int, "call conserves chips", PokerEngine.chipTotal(callGame), PokerEngine.chipTotal(called));
  switch (PokerEngine.applyAction(callGame, 0, Check)) {
  | Error(_) => ()
  | Ok(_) => Alcotest.fail("Check must not replace a required call")
  };

  let raiseGame = fundedBettingGame();
  let raised = getOk(PokerEngine.applyAction(raiseGame, 0, RaiseTo(60)));
  Alcotest.check(Alcotest.int, "raise target is total street wager", 60, raised.players[0].streetCommitted);
  Alcotest.check(Alcotest.int, "raise deducts target from stack", 940, raised.players[0].chips);
  Alcotest.check(Alcotest.int, "raise updates current bet", 60, raised.currentBet);
  Alcotest.check(Alcotest.int, "raise records last full increment", 40, raised.minRaise);
  Alcotest.check(Alcotest.int, "next minimum raise uses increment", 100,
    PokerEngine.legalOptions(raised, 1).minRaiseTo);
  Alcotest.check(Alcotest.int, "raise conserves chips", PokerEngine.chipTotal(raiseGame), PokerEngine.chipTotal(raised));
};

let shortAllInKeepsMinimumRaise = () => {
  let game = shortRaiseGame();
  let allIn = getOk(PokerEngine.applyAction(game, 0, AllIn));
  Alcotest.check(Alcotest.bool, "short stack is all-in", true, allIn.players[0].allIn);
  Alcotest.check(Alcotest.int, "short all-in spends stack", 0, allIn.players[0].chips);
  Alcotest.check(Alcotest.int, "short all-in increases current bet", 25, allIn.currentBet);
  let options = PokerEngine.legalOptions(allIn, 1);
  Alcotest.check(Alcotest.bool, "unacted player retains raise right", true, options.canRaise);
  Alcotest.check(Alcotest.int, "short all-in preserves the prior full raise increment", 45, options.minRaiseTo);
  switch (PokerEngine.applyAction(allIn, 1, RaiseTo(40))) {
  | Error(_) => ()
  | Ok(_) => Alcotest.fail("A raise below the prior minimum target must be rejected")
  };
  let raised = getOk(PokerEngine.applyAction(allIn, 1, RaiseTo(45)));
  Alcotest.check(Alcotest.int, "completion uses new total contribution", 45, raised.players[1].streetCommitted);
  Alcotest.check(Alcotest.int, "completion deducts only additional chips", 975, raised.players[1].chips);
  Alcotest.check(Alcotest.int, "short all-in sequence conserves chips", PokerEngine.chipTotal(game), PokerEngine.chipTotal(raised));
};

let foldWinPaysRemainingContender = () => {
  let setup = fundedBettingGame();
  let players = Array.init(PokerEngine.playerCount, index =>
    index >= 2 ? {...setup.players[index], folded: true} : setup.players[index]
  );
  let game = {...setup, players};
  let completed = getOk(PokerEngine.applyAction(game, 0, Fold));
  Alcotest.check(Alcotest.bool, "last fold ends hand", true, completed.handOver);
  Alcotest.check(Alcotest.bool, "fold win needs no actor", true, completed.currentPlayer == None);
  Alcotest.check(Alcotest.int, "remaining contender receives pot", 1100, completed.players[1].chips);
  Alcotest.check(Alcotest.int, "fold payout conserves chips", PokerEngine.chipTotal(game), PokerEngine.chipTotal(completed));
  Alcotest.check(Alcotest.int, "fold payout clears pot", 0, PokerEngine.potTotal(completed));
};

let automaticAllInRunout = () => {
  let base = PokerEngine.createGame();
  let deck = PokerEngine.freshDeck();
  let players = Array.init(PokerEngine.playerCount, index => {
    ...base.players[index],
    chips: index == 0 ? 980 : 0,
    committed: 20,
    streetCommitted: 20,
    allIn: index != 0,
    hasActed: false,
    holeCards: [|deck[index * 2], deck[index * 2 + 1]|],
  });
  let game = {
    ...base,
    players,
    dealer: 5,
    smallBlind: 0,
    bigBlind: 1,
    currentPlayer: Some(0),
    currentBet: 20,
    minRaise: 20,
    street: Preflop,
    deck,
    nextCard: 12,
    handNumber: 1,
  };
  let completed = getOk(PokerEngine.applyAction(game, 0, Check));
  Alcotest.check(Alcotest.bool, "no further betting decision is required", true, completed.handOver);
  Alcotest.check(Alcotest.bool, "runout reaches showdown", true, completed.street == Showdown);
  Alcotest.check(Alcotest.int, "runout deals five community cards", 5, Array.length(completed.board));
  Alcotest.check(Alcotest.bool, "showdown has no current actor", true, completed.currentPlayer == None);
  Alcotest.check(Alcotest.int, "automatic runout conserves chips", PokerEngine.chipTotal(game), PokerEngine.chipTotal(completed));
};

let sixHandedFoldWinAndNextHand = () => {
  let dealt = getOk(PokerEngine.startHand(PokerEngine.createGame(~seed=42, ())));
  Alcotest.check(Alcotest.bool, "six-handed action starts left of big blind", true, dealt.currentPlayer == Some(3));
  let third = getOk(PokerEngine.applyAction(dealt, 3, Fold));
  let fourth = getOk(PokerEngine.applyAction(third, 4, Fold));
  let fifth = getOk(PokerEngine.applyAction(fourth, 5, Fold));
  let called = getOk(PokerEngine.applyAction(fifth, 0, Call));
  let smallBlindFolded = getOk(PokerEngine.applyAction(called, 1, Fold));
  let completed = getOk(PokerEngine.applyAction(smallBlindFolded, 2, Fold));
  Alcotest.check(Alcotest.bool, "five opponent folds end the hand", true, completed.handOver);
  Alcotest.check(Alcotest.int, "human receives all contributed chips", 1030, completed.players[0].chips);
  Alcotest.check(Alcotest.int, "six-handed fold payout conserves chips", 6000, PokerEngine.chipTotal(completed));
  Alcotest.check(Alcotest.int, "six-handed fold payout clears pot", 0, PokerEngine.potTotal(completed));

  let next = getOk(PokerEngine.startHand(completed));
  Alcotest.check(Alcotest.int, "next hand increments hand number", 2, next.handNumber);
  Alcotest.check(Alcotest.int, "six-handed dealer rotates", 1, next.dealer);
  Alcotest.check(Alcotest.int, "six-handed small blind rotates", 2, next.smallBlind);
  Alcotest.check(Alcotest.int, "six-handed big blind rotates", 3, next.bigBlind);
  Alcotest.check(Alcotest.bool, "next action starts left of new big blind", true, next.currentPlayer == Some(4));
  Alcotest.check(Alcotest.int, "next hand deals twelve fresh hole cards", 12,
    next.players->Js.Array.reduce(~f=(total, player) => total + Array.length(player.holeCards), ~init=0));
  Alcotest.check(Alcotest.int, "next hand clears the board", 0, Array.length(next.board));
  Alcotest.check(Alcotest.int, "next hand preserves total chips", 6000, PokerEngine.chipTotal(next));
};

let headsUpBlindsTurnOrderAndRotation = () => {
  let base = PokerEngine.createGame(~seed=97, ());
  let players = Array.init(PokerEngine.playerCount, index => {
    ...base.players[index],
    chips: index == 0 || index == 2 ? 3000 : 0,
  });
  let lobby = {...base, players, dealer: 5};
  let dealt = getOk(PokerEngine.startHand(lobby));
  Alcotest.check(Alcotest.int, "next funded seat deals", 0, dealt.dealer);
  Alcotest.check(Alcotest.int, "heads-up dealer posts small blind", 0, dealt.smallBlind);
  Alcotest.check(Alcotest.int, "other funded seat posts big blind", 2, dealt.bigBlind);
  Alcotest.check(Alcotest.int, "dealer posts only the small blind", 10, dealt.players[0].committed);
  Alcotest.check(Alcotest.int, "other seat posts only the big blind", 20, dealt.players[2].committed);
  Alcotest.check(Alcotest.bool, "small blind acts first preflop", true, dealt.currentPlayer == Some(0));
  Alcotest.check(Alcotest.int, "only funded seats receive cards", 4,
    dealt.players->Js.Array.reduce(~f=(total, player) => total + Array.length(player.holeCards), ~init=0));

  let called = getOk(PokerEngine.applyAction(dealt, 0, Call));
  Alcotest.check(Alcotest.bool, "big blind receives the option", true, called.currentPlayer == Some(2));
  let flop = getOk(PokerEngine.applyAction(called, 2, Check));
  Alcotest.check(Alcotest.bool, "call and option advance to flop", true, flop.street == Flop);
  Alcotest.check(Alcotest.bool, "big blind acts first on flop", true, flop.currentPlayer == Some(2));
  let flopChecked = getOk(PokerEngine.applyAction(flop, 2, Check));
  Alcotest.check(Alcotest.bool, "dealer acts second on flop", true, flopChecked.currentPlayer == Some(0));
  let turn = getOk(PokerEngine.applyAction(flopChecked, 0, Check));
  Alcotest.check(Alcotest.bool, "flop checks advance to turn", true, turn.street == Turn);
  Alcotest.check(Alcotest.bool, "big blind acts first on turn", true, turn.currentPlayer == Some(2));
  let turnChecked = getOk(PokerEngine.applyAction(turn, 2, Check));
  let river = getOk(PokerEngine.applyAction(turnChecked, 0, Check));
  Alcotest.check(Alcotest.bool, "turn checks advance to river", true, river.street == River);
  Alcotest.check(Alcotest.bool, "big blind acts first on river", true, river.currentPlayer == Some(2));
  let riverChecked = getOk(PokerEngine.applyAction(river, 2, Check));
  let completed = getOk(PokerEngine.applyAction(riverChecked, 0, Check));
  Alcotest.check(Alcotest.bool, "river checks reach showdown", true, completed.street == Showdown && completed.handOver);
  Alcotest.check(Alcotest.int, "showdown deals the full board", 5, Array.length(completed.board));
  Alcotest.check(Alcotest.int, "heads-up showdown conserves chips", 6000, PokerEngine.chipTotal(completed));
  Alcotest.check(Alcotest.int, "completed hand clears pot", 0, PokerEngine.potTotal(completed));
  Alcotest.check(Alcotest.bool, "completed hand clears all contributions", true,
    completed.players->Js.Array.every(~f=player => player.committed == 0 && player.streetCommitted == 0));

  let next = getOk(PokerEngine.startHand(completed));
  Alcotest.check(Alcotest.int, "next heads-up dealer rotates", 2, next.dealer);
  Alcotest.check(Alcotest.int, "new dealer posts small blind", 2, next.smallBlind);
  Alcotest.check(Alcotest.int, "other seat now posts big blind", 0, next.bigBlind);
  Alcotest.check(Alcotest.bool, "new small blind acts first", true, next.currentPlayer == Some(2));
  Alcotest.check(Alcotest.int, "next hand also conserves chips", 6000, PokerEngine.chipTotal(next));
};

let cumulativeShortAllInsReopenRaise = () => {
  let base = PokerEngine.createGame();
  let players = Array.init(PokerEngine.playerCount, index => {
    ...base.players[index],
    chips: index == 1 ? 150 : index == 2 ? 200 : 1000,
    folded: index >= 4,
    busted: index >= 4,
  });
  let game = {
    ...base,
    players,
    dealer: 5,
    smallBlind: 0,
    bigBlind: 1,
    currentPlayer: Some(0),
    currentBet: 0,
    minRaise: 100,
    street: Preflop,
    handNumber: 1,
  };
  let bet = getOk(PokerEngine.applyAction(game, 0, Bet(100)));
  let firstShove = getOk(PokerEngine.applyAction(bet, 1, AllIn));
  let secondShove = getOk(PokerEngine.applyAction(firstShove, 2, AllIn));
  let called = getOk(PokerEngine.applyAction(secondShove, 3, Call));
  Alcotest.check(Alcotest.bool, "A faces action after two short shoves and a call", true, called.currentPlayer == Some(0));
  let options = PokerEngine.legalOptions(called, 0);
  Alcotest.check(Alcotest.bool, "cumulative full increment reopens raise", true, options.canRaise);
  Alcotest.check(Alcotest.bool, "cumulative full increment reopens all-in", true, options.canAllIn);
  Alcotest.check(Alcotest.int, "minimum re-raise is current bet plus full increment", 300, options.minRaiseTo);
  let raised = getOk(PokerEngine.applyAction(called, 0, RaiseTo(300)));
  Alcotest.check(Alcotest.int, "re-raise deducts only the additional amount", 700, raised.players[0].chips);
  Alcotest.check(Alcotest.int, "re-raise uses the new street target", 300, raised.currentBet);
  Alcotest.check(Alcotest.int, "cumulative all-in sequence conserves chips", PokerEngine.chipTotal(called), PokerEngine.chipTotal(raised));
};

let tiedPotAndOddChip = () => {
  let base = PokerEngine.createGame();
  let players = [|
    {...base.players[0], chips: 900, committed: 100, streetCommitted: 100, allIn: true,
     holeCards: [|card(14, Clubs), card(14, Diamonds)|]},
    {...base.players[1], chips: 900, committed: 100, streetCommitted: 100, allIn: true,
     holeCards: [|card(13, Hearts), card(13, Spades)|]},
    {...base.players[2], chips: 999, committed: 1, streetCommitted: 1, folded: true},
    {...base.players[3], folded: true, busted: true},
    {...base.players[4], folded: true, busted: true},
    {...base.players[5], folded: true, busted: true},
  |];
  let game = {
    ...base,
    players,
    board: [|
      card(2, Clubs), card(3, Diamonds), card(4, Hearts),
      card(5, Spades), card(6, Clubs),
    |],
    street: River,
    handNumber: 1,
    dealer: 5,
  };
  let settled = PokerEngine.settleShowdown(game);
  Alcotest.check(Alcotest.int, "first tied winner receives odd chip", 1001, settled.players[0].chips);
  Alcotest.check(Alcotest.int, "second tied winner receives equal share", 1000, settled.players[1].chips);
  Alcotest.check(Alcotest.int, "tied pot conserves chips", 6000, PokerEngine.chipTotal(settled));
};

let () =
  Alcotest.run(
    "poker-engine",
    [
      ("hand evaluation", [
        Alcotest.test_case("wheel and straight ordering", `Quick, evaluatorOrdersWheelAndPairs),
        Alcotest.test_case("full houses and equal hands", `Quick, evaluatorHandlesFullHousesAndTies),
      ]),
      ("deck", [Alcotest.test_case("shuffle contains every card once", `Quick, shuffledDeckHasEveryCardOnce)]),
      ("game transitions", [
        Alcotest.test_case("deterministic deal and chip conservation", `Quick, deterministicDealAndConservation),
        Alcotest.test_case("calls and raises update contributions", `Quick, bettingTransitions),
        Alcotest.test_case("short all-in preserves minimum raise", `Quick, shortAllInKeepsMinimumRaise),
        Alcotest.test_case("cumulative short all-ins reopen raise", `Quick, cumulativeShortAllInsReopenRaise),
        Alcotest.test_case("fold pays remaining contender", `Quick, foldWinPaysRemainingContender),
        Alcotest.test_case("all-in board runs out automatically", `Quick, automaticAllInRunout),
        Alcotest.test_case("six-handed fold win and next hand", `Quick, sixHandedFoldWinAndNextHand),
        Alcotest.test_case("heads-up blinds, turn order, and rotation", `Quick, headsUpBlindsTurnOrderAndRotation),
      ]),
      ("settlement", [
        Alcotest.test_case("main and side pots pay eligible winners", `Quick, sidePotSettlement),
        Alcotest.test_case("ties split pots and distribute odd chips", `Quick, tiedPotAndOddChip),
      ]),
    ],
  );
