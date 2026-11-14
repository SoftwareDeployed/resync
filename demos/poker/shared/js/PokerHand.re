open PokerTypes;

type value = {
  category: int,
  ranks: array(int),
};

let categoryName = category =>
  switch (category) {
  | 8 => "Straight flush"
  | 7 => "Four of a kind"
  | 6 => "Full house"
  | 5 => "Flush"
  | 4 => "Straight"
  | 3 => "Three of a kind"
  | 2 => "Two pair"
  | 1 => "One pair"
  | _ => "High card"
  };

let rankLabel = rank =>
  switch (rank) {
  | 14 => "A"
  | 13 => "K"
  | 12 => "Q"
  | 11 => "J"
  | 10 => "10"
  | value => string_of_int(value)
  };

let rankName = rank =>
  switch (rank) {
  | 14 => "ace"
  | 13 => "king"
  | 12 => "queen"
  | 11 => "jack"
  | value => string_of_int(value)
  };

let suitName = suit =>
  switch (suit) {
  | Clubs => "clubs"
  | Diamonds => "diamonds"
  | Hearts => "hearts"
  | Spades => "spades"
  };

let suitLetter = suit =>
  switch (suit) {
  | Clubs => "C"
  | Diamonds => "D"
  | Hearts => "H"
  | Spades => "S"
  };

let cardLabel = card => rankLabel(card.rank) ++ suitLetter(card.suit);

let cardName = card => rankName(card.rank) ++ " of " ++ suitName(card.suit);

let compareValue = (left: value, right: value) => {
  let categoryComparison = compare(left.category, right.category);
  if (categoryComparison != 0) {
    categoryComparison;
  } else {
    let rec compareRanks = index =>
      if (index >= Array.length(left.ranks) || index >= Array.length(right.ranks)) {
        compare(Array.length(left.ranks), Array.length(right.ranks));
      } else {
        let rankComparison = compare(left.ranks[index], right.ranks[index]);
        rankComparison == 0 ? compareRanks(index + 1) : rankComparison;
      };
    compareRanks(0);
  };
};

let hasRank = (counts, rank) => rank >= 2 && rank <= 14 && counts[rank] > 0;

let straightHigh = counts => {
  let wheel = hasRank(counts, 14) && hasRank(counts, 5) && hasRank(counts, 4)
    && hasRank(counts, 3) && hasRank(counts, 2);
  let rec find = high =>
    if (high < 6) {
      wheel ? 5 : 0;
    } else if (
      hasRank(counts, high)
      && hasRank(counts, high - 1)
      && hasRank(counts, high - 2)
      && hasRank(counts, high - 3)
      && hasRank(counts, high - 4)
    ) {
      high;
    } else {
      find(high - 1);
    };
  find(14);
};

let descendingRanks = (cards: array(card)) => {
  let sorted = Js.Array.slice(~start=0, ~end_=Array.length(cards), cards);
  Belt.SortArray.stableSortInPlaceBy(sorted, (left, right) => compare(right.rank, left.rank));
  sorted->Js.Array.map(~f=card => card.rank);
};

let ranksOfCount = (counts, wanted) => {
  let ranks = ref([||]);
  for (rank in 14 downto 2) {
    if (counts[rank] == wanted) {
      ranks := Js.Array.concat(~other=[|rank|], ranks^);
    };
  };
  ranks^;
};

let highCardsExcluding = (counts, excluded) => {
  let ranks = ref([||]);
  for (rank in 14 downto 2) {
    if (counts[rank] > 0 && !Js.Array.some(~f=value => value == rank, excluded)) {
      ranks := Js.Array.concat(~other=[|rank|], ranks^);
    };
  };
  ranks^;
};

let take = (count, values) =>
  Js.Array.slice(~start=0, ~end_=min(count, Array.length(values)), values);

let evaluateFive = (cards: array(card)): value => {
  let counts = Array.make(15, 0);
  cards->Js.Array.forEach(~f=card => counts[card.rank] = counts[card.rank] + 1);
  let flush =
    Array.length(cards) == 5
    && cards->Js.Array.every(~f=card => card.suit == cards[0].suit);
  let highStraight = straightHigh(counts);
  let descending = descendingRanks(cards);
  let fours = ranksOfCount(counts, 4);
  let threes = ranksOfCount(counts, 3);
  let pairs = ranksOfCount(counts, 2);

  if (flush && highStraight > 0) {
    {category: 8, ranks: [|highStraight|]};
  } else if (Array.length(fours) > 0) {
    {
      category: 7,
      ranks: Js.Array.concat(~other=take(1, highCardsExcluding(counts, fours)), fours),
    };
  } else if (Array.length(threes) > 0 && Array.length(pairs) > 0) {
    {category: 6, ranks: [|threes[0], pairs[0]|]};
  } else if (flush) {
    {category: 5, ranks: descending};
  } else if (highStraight > 0) {
    {category: 4, ranks: [|highStraight|]};
  } else if (Array.length(threes) > 0) {
    {
      category: 3,
      ranks: Js.Array.concat(~other=take(2, highCardsExcluding(counts, threes)), threes),
    };
  } else if (Array.length(pairs) >= 2) {
    let topPairs = take(2, pairs);
    {
      category: 2,
      ranks: Js.Array.concat(~other=take(1, highCardsExcluding(counts, topPairs)), topPairs),
    };
  } else if (Array.length(pairs) == 1) {
    {
      category: 1,
      ranks: Js.Array.concat(~other=take(3, highCardsExcluding(counts, pairs)), pairs),
    };
  } else {
    {category: 0, ranks: descending};
  };
};

let evaluate = (cards: array(card)): option(value) => {
  let count = Array.length(cards);
  if (count < 5) {
    None;
  } else {
    let best = ref(None);
    for (a in 0 to count - 5) {
      for (b in a + 1 to count - 4) {
        for (c in b + 1 to count - 3) {
          for (d in c + 1 to count - 2) {
            for (e in d + 1 to count - 1) {
              let candidate = evaluateFive([|
                cards[a],
                cards[b],
                cards[c],
                cards[d],
                cards[e],
              |]);
              switch (best^) {
              | None => best := Some(candidate)
              | Some(current) =>
                if (compareValue(candidate, current) > 0) {
                  best := Some(candidate);
                }
              };
            };
          };
        };
      };
    };
    best^;
  };
};

let bestFor = (player: player, board: array(card)) =>
  evaluate(Js.Array.concat(~other=board, player.holeCards));
