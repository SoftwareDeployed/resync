let getActiveId = (url: ReasonReactRouter.url) => {
  switch (url.path) {
  | ["item", id, _] => Some(id)
  | _ => None
  };
};

let getRootlessPath = (route_root, path) =>
  if (Constants.premise_routes |> List.exists(r => r === route_root)) {
    Some(path);
  } else {
    let route_root_prefix = route_root |> String.split_on_char('/');
    //->Array.filter(value => value->String.length > 0)
    let root_idx = route_root_prefix->List.length - 1->Int.min(0);

    let rootless_path =
      switch (path->Belt.List.splitAt(root_idx)) {
      | Some((rootless_path, _)) => rootless_path
      | None => [""]
      };
    Some(rootless_path);
  };

[@react.component]
let make =
    (
      ~route_root: option(string)=?,
      ~server_url: option(ReasonReactRouter.url)=?,
    ) => {
  let initial_url = ReasonReactRouter.useUrl(~serverUrl=?server_url, ());
  let (url, set_url) = React.useState(() => initial_url);
  Js.log(initial_url.path->Belt.List.head->Belt.Option.getWithDefault("/"));
  React.useEffect0(() => {
    let watcher_id =
      ReasonReactRouter.watchUrl(new_url => {set_url(_ => new_url)});

    // Cleanup function to unsubscribe when the component unmounts
    Some(() => ReasonReactRouter.unwatchUrl(watcher_id));
  });
  let main_store = StoreContext.useStore();
  Js.log(Obj.magic(main_store));
  switch (main_store.config.premise) {
  | None => <NotFound />
  | Some(_) =>
    let rootless_path =
      switch (route_root) {
      | Some(route_root) => getRootlessPath(route_root, url.path)
      | None => Some([""])
      };
    switch (rootless_path) {
    | Some(["item", _])
    | Some([""]) => <Landing />
    | _ => <NotFound />
    };
  };
};
