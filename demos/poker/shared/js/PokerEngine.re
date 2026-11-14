open PokerTypes;

let startingChips = 1000;
let smallBlindAmount = 10;
let bigBlindAmount = 20;
let historyLimit = 24;
let playerCount = 6;
let humanIndex = 0;

let names = [|"You", "Ada", "Babbage", "Curie", "Dijkstra", "Hopper"|];

let makePlayer = (id, name): player => {
  id,
  name,
  chips: startingChips,
  holeCards: [||],
  folded: false,
  allIn: false,
  busted: false,
  committed: 0,
  streetCommitted: 0,
  hasActed: false,
  canRaise: true,
  lastAction: "",
};

let createGame = (~seed=731_927, ()) => {
  players: Array.init(playerCount, index => makePlayer(index, names[index])),
  dealer: playerCount - 1,
  smallBlind: 1,
  bigBlind: 2,
  currentPlayer: None,
  currentBet: 0,
  minRaise: bigBlindAmount,
  street: Lobby,
  board: [||],
  deck: [||],
  nextCard: 0,
  handNumber: 0,
  handOver: false,
  history: [|"Take a seat. Deal when you are ready."|],
  result: "Six-handed Texas Hold'em. Play chips only.",
  rngSeed: seed,
};

let replacePlayer = (players, index, replacement) =>
  Array.init(Array.length(players), position => position == index ? replacement : players[position]);

let mapPlayers = (game: game, mapper) =>
  Array.init(Array.length(game.players), index => mapper(index, game.players[index]));

let addHistory = (game: game, entry) => {
  let items = Js.Array.concat(~other=game.history, [|entry|]);
  let history = Js.Array.slice(~start=0, ~end_=min(historyLimit, Array.length(items)), items);
  {...game, history};
};

let potTotal = (game: game) =>
  game.players->Js.Array.reduce(~f=(total, player) => total + player.committed, ~init=0);

let chipTotal = (game: game) =>
  game.players->Js.Array.reduce(
    ~f=(total, player) => total + player.chips + player.committed,
    ~init=0,
  );

let streetName = street =>
  switch (street) {
  | Lobby => "Lobby"
  | Preflop => "Preflop"
  | Flop => "Flop"
  | Turn => "Turn"
  | River => "River"
  | Showdown => "Showdown"
  };

let streetNumber = street =>
  switch (street) {
  | Lobby => 0
  | Preflop => 1
  | Flop => 2
  | Turn => 3
  | River => 4
  | Showdown => 5
  };

let freshDeck = () =>
  Array.init(52, index => {
    let suit =
      switch (index mod 4) {
      | 0 => Clubs
      | 1 => Diamonds
      | 2 => Hearts
      | _ => Spades
      };
    {rank: index / 4 + 2, suit};
  });

let nextRandom = seed => {
  let next = seed * 48_271 mod 2_147_483_647;
  let next = next == 0 ? 1 : next;
  (next, next);
};

let shuffledDeck = seed => {
  let deck = freshDeck();
  let currentSeed = ref(seed <= 0 ? 1 : seed);
  for (index in 51 downto 1) {
    let (nextSeed, randomValue) = nextRandom(currentSeed^);
    let other = randomValue mod (index + 1);
    let held = deck[index];
    deck[index] = deck[other];
    deck[other] = held;
    currentSeed := nextSeed;
  };
  (deck, currentSeed^);
};

let canAct = (player: player) =>
  !player.folded && !player.allIn && !player.busted && player.chips > 0;

let isContender = (player: player) => !player.folded && !player.busted;

let fundedCount = (game: game) =>
  game.players->Js.Array.filter(~f=player => player.chips > 0)->Array.length;

let contenderCount = (game: game) =>
  game.players->Js.Array.filter(~f=isContender)->Array.length;

let nextWhere = (predicate, players, origin) => {
  let rec search = offset =>
    if (offset > Array.length(players)) {
      None;
    } else {
      let index = (origin + offset) mod Array.length(players);
      predicate(players[index]) ? Some(index) : search(offset + 1);
    };
  search(1);
};

let legalOptions = (game: game, index): legalOptions => {
  let empty = {
    canFold: false,
    canCheck: false,
    callAmount: 0,
    canCall: false,
    canBet: false,
    minBet: bigBlindAmount,
    maxBet: 0,
    canRaise: false,
    minRaiseTo: game.currentBet + game.minRaise,
    maxRaiseTo: 0,
    canAllIn: false,
  };
  if (game.handOver || game.currentPlayer != Some(index) || index < 0 || index >= playerCount) {
    empty;
  } else {
    let player = game.players[index];
    if (!canAct(player)) {
      empty;
    } else {
      let toCall = max(0, game.currentBet - player.streetCommitted);
      let maxTarget = player.streetCommitted + player.chips;
      let canBet = game.currentBet == 0 && player.canRaise && maxTarget >= bigBlindAmount;
      let nominalRaise = game.currentBet + game.minRaise;
      let canRaise =
        game.currentBet > 0
        && player.canRaise
        && player.chips > toCall
        && maxTarget >= nominalRaise;
      let allInRaises = maxTarget > game.currentBet;
      let canAllIn =
        player.chips > 0
        && (
          player.chips <= toCall
          || (game.currentBet == 0 && player.canRaise)
          || (game.currentBet > 0 && allInRaises && player.canRaise)
        );
      {
        canFold: true,
        canCheck: toCall == 0,
        canCall: toCall > 0,
        callAmount: min(toCall, player.chips),
        canBet,
        minBet: bigBlindAmount,
        maxBet: maxTarget,
        canRaise,
        minRaiseTo: nominalRaise,
        maxRaiseTo: maxTarget,
        canAllIn,
      };
    };
  };
};

let actionName = action =>
  switch (action) {
  | Fold => "fold"
  | Check => "check"
  | Call => "call"
  | Bet(_) => "bet"
  | RaiseTo(_) => "raise"
  | AllIn => "all-in"
  };

let wager = (player: player, target) => {
  let delta = target - player.streetCommitted;
  {
    ...player,
    chips: player.chips - delta,
    committed: player.committed + delta,
    streetCommitted: target,
    allIn: player.chips == delta,
    hasActed: true,
    canRaise: false,
  };
};

let rec progress = (game: game) =>
  if (contenderCount(game) == 1) {
    awardFold(game);
  } else if (bettingRoundComplete(game)) {
    advanceStreet(game);
  } else {
    let origin =
      switch (game.currentPlayer) {
      | Some(index) => index
      | None => game.dealer
      };
    let needsAction = player =>
      canAct(player) && (!player.hasActed || player.streetCommitted != game.currentBet);
    let next = nextWhere(needsAction, game.players, origin);
    {...game, currentPlayer: next};
  }
and bettingRoundComplete = (game: game) => {
  let actors = game.players->Js.Array.filter(~f=canAct);
  switch (actors) {
  | [||] => true
  | [|onlyActor|] => onlyActor.streetCommitted == game.currentBet
  | _ =>
    actors->Js.Array.every(~f=player =>
      player.hasActed && player.streetCommitted == game.currentBet
    )
  };
}
and awardFold = (game: game) => {
  let winnerIndex =
    switch (nextWhere(isContender, game.players, -1)) {
    | Some(index) => index
    | None => 0
    };
  let amount = potTotal(game);
  let winner = game.players[winnerIndex];
  let updated = {
    ...winner,
    chips: winner.chips + amount,
    lastAction: "Won " ++ string_of_int(amount),
  };
  let awarded = replacePlayer(game.players, winnerIndex, updated);
  let players = Array.init(Array.length(awarded), index => {
    ...awarded[index],
    committed: 0,
    streetCommitted: 0,
  });
  let result = winner.name ++ " wins " ++ string_of_int(amount) ++ ". Everyone else folded.";
  {
    ...game,
    players,
    currentPlayer: None,
    handOver: true,
    result,
  }->addHistory(result);
}
and drawBoard = (game: game, count) => {
  let drawn = Array.init(count, offset => game.deck[game.nextCard + offset]);
  {
    ...game,
    board: Js.Array.concat(~other=drawn, game.board),
    nextCard: game.nextCard + count,
  };
}
and resetStreet = (game: game, street) => {
  let players = mapPlayers(game, (_index, player) => {
    ...player,
    streetCommitted: 0,
    hasActed: false,
    canRaise: true,
    lastAction: player.folded ? "Fold" : "",
  });
  {
    ...game,
    players,
    street,
    currentBet: 0,
    minRaise: bigBlindAmount,
    currentPlayer: Some(game.dealer),
  };
}
and advanceStreet = (game: game) =>
  switch (game.street) {
  | Preflop =>
    let next = game->drawBoard(3)->resetStreet(Flop);
    let labels = next.board->Js.Array.map(~f=PokerHand.cardLabel);
    let text = "Flop: " ++ labels->Js.Array.reduce(~f=(a, b) => a ++ " " ++ b, ~init="");
    progress(next->addHistory(text));
  | Flop =>
    let next = game->drawBoard(1)->resetStreet(Turn);
    progress(next->addHistory("Turn: " ++ PokerHand.cardLabel(next.board[3])));
  | Turn =>
    let next = game->drawBoard(1)->resetStreet(River);
    progress(next->addHistory("River: " ++ PokerHand.cardLabel(next.board[4])));
  | River => settleShowdown(game)
  | Lobby
  | Showdown => game
  }
and settleShowdown = (game: game) => {
  let levels = ref([||]);
  game.players->Js.Array.forEach(~f=player =>
    if (player.committed > 0 && !Js.Array.some(~f=level => level == player.committed, levels^)) {
      levels := Js.Array.concat(~other=[|player.committed|], levels^);
    }
  );
  Belt.SortArray.stableSortInPlaceBy(levels^, (left, right) => compare(left, right));

  let winnings = Array.make(playerCount, 0);
  let handValues = Array.make(playerCount, None);
  let previousLevel = ref(0);

  (levels^)->Js.Array.forEach(~f=level => {
    let contributors =
      game.players->Js.Array.filter(~f=player => player.committed >= level)->Array.length;
    let amount = (level - previousLevel^) * contributors;
    previousLevel := level;
    if (amount > 0) {
      let eligible =
        Array.init(playerCount, index => index)
        ->Js.Array.filter(~f=index => {
            let player = game.players[index];
            isContender(player) && player.committed >= level;
          });
      let best = ref(None);
      eligible->Js.Array.forEach(~f=index => {
        let value = PokerHand.bestFor(game.players[index], game.board);
        handValues[index] = value;
        switch (value, best^) {
        | (Some(candidate), None) => best := Some(candidate)
        | (Some(candidate), Some(current)) =>
          if (PokerHand.compareValue(candidate, current) > 0) {
            best := Some(candidate);
          }
        | _ => ()
        };
      });
      let winners =
        switch (best^) {
        | None => [||]
        | Some(bestValue) =>
          eligible->Js.Array.filter(~f=index =>
            switch (handValues[index]) {
            | Some(value) => PokerHand.compareValue(value, bestValue) == 0
            | None => false
            }
          )
        };
      let winnerCount = Array.length(winners);
      if (winnerCount > 0) {
        let share = amount / winnerCount;
        let remainder = ref(amount mod winnerCount);
        winners->Js.Array.forEach(~f=index => winnings[index] = winnings[index] + share);
        for (offset in 1 to playerCount) {
          let index = (game.dealer + offset) mod playerCount;
          if (remainder^ > 0 && Js.Array.some(~f=winner => winner == index, winners)) {
            winnings[index] = winnings[index] + 1;
            remainder := remainder^ - 1;
          };
        };
      };
    };
  });

  let players = mapPlayers(game, (index, player) => {
    let won = winnings[index];
    let action =
      if (won > 0) {
        let category =
          switch (handValues[index]) {
          | Some(value) => " • " ++ PokerHand.categoryName(value.category)
          | None => ""
          };
        "Won " ++ string_of_int(won) ++ category;
      } else if (isContender(player)) {
        switch (handValues[index]) {
        | Some(value) => PokerHand.categoryName(value.category)
        | None => ""
        };
      } else {
        player.lastAction;
      };
    {
      ...player,
      chips: player.chips + won,
      committed: 0,
      streetCommitted: 0,
      lastAction: action,
    };
  });

  let summaries = ref([||]);
  for (index in 0 to playerCount - 1) {
    if (winnings[index] > 0) {
      let category =
        switch (handValues[index]) {
        | Some(value) => " • " ++ PokerHand.categoryName(value.category)
        | None => ""
        };
      summaries := Js.Array.concat(
        ~other=[|game.players[index].name ++ " wins " ++ string_of_int(winnings[index]) ++ category|],
        summaries^,
      );
    };
  };
  let result =
    if (Array.length(summaries^) == 0) {
      "Showdown complete.";
    } else {
      (summaries^)->Js.Array.reduce(~f=(text, item) => text == "" ? item : text ++ "; " ++ item, ~init="");
    };
  {
    ...game,
    players,
    street: Showdown,
    handOver: true,
    currentPlayer: None,
    result,
  }->addHistory(result);
};

let applyAction = (game: game, index: int, action: action): result(game, string) => {
  let options = legalOptions(game, index);
  let player = game.players[index];
  let valid =
    switch (action) {
    | Fold => options.canFold
    | Check => options.canCheck
    | Call => options.canCall
    | Bet(target) => options.canBet && target >= options.minBet && target <= options.maxBet
    | RaiseTo(target) =>
      options.canRaise && target >= options.minRaiseTo && target <= options.maxRaiseTo
    | AllIn => options.canAllIn
    };
  if (!valid) {
    Error("That " ++ actionName(action) ++ " action is not legal now.");
  } else {
    let toCall = max(0, game.currentBet - player.streetCommitted);
    let (updatedPlayer, label, increased, fullRaise) =
      switch (action) {
      | Fold =>
        ({...player, folded: true, hasActed: true, canRaise: false, lastAction: "Fold"}, "folds", false, false)
      | Check =>
        ({...player, hasActed: true, canRaise: false, lastAction: "Check"}, "checks", false, false)
      | Call =>
        let amount = min(toCall, player.chips);
        let updated = wager(player, player.streetCommitted + amount);
        (
          {...updated, lastAction: amount < toCall ? "All-in " ++ string_of_int(amount) : "Call " ++ string_of_int(amount)},
          amount < toCall ? "calls all-in for " ++ string_of_int(amount) : "calls " ++ string_of_int(amount),
          false,
          false,
        )
      | Bet(target) =>
        let updated = wager(player, target);
        ({...updated, lastAction: "Bet " ++ string_of_int(target)}, "bets " ++ string_of_int(target), true, true)
      | RaiseTo(target) =>
        let updated = wager(player, target);
        ({...updated, lastAction: "Raise to " ++ string_of_int(target)}, "raises to " ++ string_of_int(target), true, true)
      | AllIn =>
        let amount = player.chips;
        let target = player.streetCommitted + amount;
        let updated = wager(player, target);
        let increased = target > game.currentBet;
        let fullRaise =
          increased
          && (
            game.currentBet == 0
              ? target >= bigBlindAmount
              : target - game.currentBet >= game.minRaise
          );
        (
          {...updated, lastAction: "All-in " ++ string_of_int(amount)},
          "moves all-in for " ++ string_of_int(amount),
          increased,
          fullRaise,
        )
      };
    let players = replacePlayer(game.players, index, updatedPlayer);
    let currentBet =
      switch (action) {
      | Bet(target)
      | RaiseTo(target) => target
      | AllIn => max(game.currentBet, updatedPlayer.streetCommitted)
      | _ => game.currentBet
      };
    let minRaise =
      if (fullRaise) {
        currentBet - game.currentBet;
      } else {
        game.minRaise;
      };
    let flaggedPlayers =
      players->Js.Array.map(~f=other =>
        if (other.id == index || other.folded || other.busted) {
          other;
        } else if (increased) {
          {...other, hasActed: false, canRaise: fullRaise ? true : other.canRaise};
        } else {
          other;
        }
      );
    let next = {
      ...game,
      players: flaggedPlayers,
      currentBet,
      minRaise,
      currentPlayer: Some(index),
    };
    Ok(progress(next->addHistory(player.name ++ " " ++ label)));
  };
};

let startHand = (game: game): result(game, string) => {
  if (!game.handOver && game.handNumber > 0) {
    Error("Finish this hand before dealing another.");
  } else if (game.players[humanIndex].chips <= 0) {
    Error("You are out of chips. Restart the table to play again.");
  } else if (fundedCount(game) < 2) {
    Error("At least two funded players are needed. Restart the table.");
  } else {
    let (deck, rngSeed) = shuffledDeck(game.rngSeed);
    let players = mapPlayers(game, (_index, player) => {
      let busted = player.chips <= 0;
      {
        ...player,
        holeCards: [||],
        folded: busted,
        busted,
        allIn: false,
        committed: 0,
        streetCommitted: 0,
        hasActed: false,
        canRaise: true,
        lastAction: busted ? "Busted" : "Waiting",
      };
    });
    let dealer =
      switch (nextWhere(player => player.chips > 0, players, game.dealer)) {
      | Some(index) => index
      | None => humanIndex
      };
    let smallBlind =
      if (fundedCount(game) == 2) {
        dealer;
      } else {
        switch (nextWhere(player => player.chips > 0, players, dealer)) {
        | Some(index) => index
        | None => dealer
        };
      };
    let bigBlind =
      switch (nextWhere(player => player.chips > 0, players, smallBlind)) {
      | Some(index) => index
      | None => smallBlind
      };
    let headNumber = game.handNumber + 1;
    let initial = {
      ...game,
      players,
      dealer,
      smallBlind,
      bigBlind,
      currentPlayer: Some(bigBlind),
      currentBet: 0,
      minRaise: bigBlindAmount,
      street: Preflop,
      board: [||],
      deck,
      nextCard: 0,
      handNumber: headNumber,
      handOver: false,
      result: "Hand " ++ string_of_int(headNumber) ++ " is underway.",
      rngSeed,
    };
    let smallBlind = initial.smallBlind;

    let dealt = ref(initial);
    for (_round in 0 to 1) {
      for (offset in 1 to playerCount) {
        let index = (dealer + offset) mod playerCount;
        let player = dealt^.players[index];
        if (!player.busted) {
          let card = dealt^.deck[dealt^.nextCard];
          dealt := {
            ...dealt^,
            players: replacePlayer(
              dealt^.players,
              index,
              {...player, holeCards: Js.Array.concat(~other=[|card|], player.holeCards)},
            ),
            nextCard: dealt^.nextCard + 1,
          };
        };
      };
    };

    let postBlind = (state: game, index, amount, label) => {
      let player = state.players[index];
      let posted = min(amount, player.chips);
      let updated = {
        ...player,
        chips: player.chips - posted,
        committed: player.committed + posted,
        streetCommitted: player.streetCommitted + posted,
        allIn: player.chips == posted,
        lastAction: label ++ " " ++ string_of_int(posted),
      };
      {
        ...state,
        players: replacePlayer(state.players, index, updated),
        currentBet: max(state.currentBet, updated.streetCommitted),
      };
    };
    let withSmall = postBlind(dealt^, smallBlind, smallBlindAmount, "Small blind");
    let withBig = postBlind(withSmall, bigBlind, bigBlindAmount, "Big blind");
    let withBig = {...withBig, currentBet: max(bigBlindAmount, withBig.currentBet)};
    let blindHistory =
      "Hand " ++ string_of_int(headNumber)
      ++ " • " ++ withBig.players[dealer].name ++ " deals; blinds "
      ++ string_of_int(withBig.players[smallBlind].streetCommitted) ++ "/"
      ++ string_of_int(withBig.players[bigBlind].streetCommitted);
    Ok(progress(withBig->addHistory(blindHistory)));
  };
};

let restart = (~seed=731_927, ()) => createGame(~seed, ());
