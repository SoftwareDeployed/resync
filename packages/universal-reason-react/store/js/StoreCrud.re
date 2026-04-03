type patch('row) =
  | Upsert('row)
  | Delete(string);

let upsert = (~getId: 'row => string, items: array('row), newItem: 'row): array('row) => {
  let exists =
    items |> Js.Array.some(~f=(i: 'row) => getId(i) === getId(newItem));
  if (exists) {
    items
    |> Js.Array.map(~f=(i: 'row) => getId(i) === getId(newItem) ? newItem : i);
  } else {
    Js.Array.concat(~other=[|newItem|], items);
  };
};

let remove = (~getId: 'row => string, items: array('row), id: string): array('row) =>
  items |> Js.Array.filter(~f=(i: 'row) => getId(i) !== id);

let decodePatch =
    (~table: string, ~decodeRow: StoreJson.json => 'row, ()): StorePatch.decoder(patch('row)) =>
  StorePatch.Pg.decodeAs(
    ~table,
    ~decodeRow,
    ~insert=data => Upsert(data),
    ~update=data => Upsert(data),
    ~delete=id => Delete(id),
    (),
  );

let updateOfPatch =
    (~getId: 'row => string, ~getItems: 'config => array('row), ~setItems: ('config, array('row)) => 'config, patch: patch('row)):
    ('config => 'config) =>
  switch (patch) {
  | Upsert(newItem) =>
    config => setItems(config, upsert(~getId, getItems(config), newItem))
  | Delete(id) =>
    config => setItems(config, remove(~getId, getItems(config), id))
  };
