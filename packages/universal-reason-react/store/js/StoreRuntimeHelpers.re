let replayActions = (~confirmed, ~records, ~reduce) => {
  let sorted = StoreActionLedger.sortByEnqueuedAt(records);
  let length = Array.length(sorted);
  let rec loop = (index, current) =>
    if (index >= length) {
      current;
    } else {
      let record: StoreActionLedger.t = sorted[index];
      switch (StoreActionLedger.statusOfString(record.status)) {
      | Pending | Syncing =>
          let next = reduce(current, record);
          loop(index + 1, next);
      | _ => loop(index + 1, current)
      };
    };
  loop(0, confirmed);
};

let selectHydrationBase = (~initialState, ~persistedState, ~timestampOfState) => {
  switch (persistedState) {
  | None => initialState
  | Some(persisted) =>
      let initialTimestamp = timestampOfState(initialState);
      let persistedTimestamp = timestampOfState(persisted);
      if (persistedTimestamp > initialTimestamp) {
        persisted;
      } else {
        initialState;
      };
  };
};

let rejectStaleCacheResult = (~currentConfirmedState, ~cachedState, ~timestampOfState) =>
  timestampOfState(cachedState) <= timestampOfState(currentConfirmedState);

let filterResumableRecords = (records) => {
  records
  ->Js.Array.filter(~f=(record: StoreActionLedger.t) =>
       switch (StoreActionLedger.statusOfString(record.status)) {
       | Pending | Syncing => true
       | _ => false
       }
     );
};

let getPrunableAckedActionIds = (~confirmedTimestamp, ~records) => {
  records
  ->Js.Array.filter(~f=(record: StoreActionLedger.t) =>
       switch (StoreActionLedger.statusOfString(record.status)) {
       | Acked => UUID.timestamp(record.id) <= confirmedTimestamp
       | _ => false
       }
     )
  ->Js.Array.map(~f=(record: StoreActionLedger.t) => record.id);
};
