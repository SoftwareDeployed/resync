type suit =
  | Clubs
  | Diamonds
  | Hearts
  | Spades;

type card = {
  rank: int,
  suit,
};

type street =
  | Lobby
  | Preflop
  | Flop
  | Turn
  | River
  | Showdown;

type action =
  | Fold
  | Check
  | Call
  | Bet(int)
  | RaiseTo(int)
  | AllIn;

type player = {
  id: int,
  name: string,
  chips: int,
  holeCards: array(card),
  folded: bool,
  allIn: bool,
  busted: bool,
  committed: int,
  streetCommitted: int,
  hasActed: bool,
  canRaise: bool,
  lastAction: string,
};

type game = {
  players: array(player),
  dealer: int,
  smallBlind: int,
  bigBlind: int,
  currentPlayer: option(int),
  currentBet: int,
  minRaise: int,
  street,
  board: array(card),
  deck: array(card),
  nextCard: int,
  handNumber: int,
  handOver: bool,
  history: array(string),
  result: string,
  rngSeed: int,
};

type legalOptions = {
  canFold: bool,
  canCheck: bool,
  canCall: bool,
  callAmount: int,
  canBet: bool,
  minBet: int,
  maxBet: int,
  canRaise: bool,
  minRaiseTo: int,
  maxRaiseTo: int,
  canAllIn: bool,
};
