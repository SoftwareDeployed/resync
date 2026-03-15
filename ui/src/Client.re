[@platform js]
module Socket = {
  let base_url = "http://localhost:8899";
  //[@mel.scope ("process", "env")] external base_url: string = "API_BASE_URL";
  external globalThis: Js.Dict.t(int) = "globalThis";
  external setInterval: (. unit => unit, int) => int = "setInterval";

  type websocket_state = {
    last_ping: float,
    last_pong: float,
    updated_at: float,
  };

  type patch_message = {
    type_: string,
    table_: string,
    action: string,
    data: option(Config.InventoryItem.t),
    id: option(string),
  };

  /* Parse inventory item from patch payload (without period_list - we merge from store) */
  let inventory_item_from_json = (json: Js.Json.t) => {
    let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
    {
      Config.InventoryItem.description: (
        Obj.magic(Js.Dict.unsafeGet(dict, "description")): string
      ),
      id: (Obj.magic(Js.Dict.unsafeGet(dict, "id")): string),
      name: (Obj.magic(Js.Dict.unsafeGet(dict, "name")): string),
      quantity: (Obj.magic(Js.Dict.unsafeGet(dict, "quantity")): int),
      premise_id: (Obj.magic(Js.Dict.unsafeGet(dict, "premise_id")): string),
      period_list: [||],
    };
  };

  let parse_patch = (json: Js.Json.t): option(patch_message) =>
    try({
      let dict: Js.Dict.t(Js.Json.t) = Obj.magic(json);
      switch (Js.Dict.get(dict, "type")) {
      | Some(typeJson) =>
        let typeStr: string = Obj.magic(typeJson);
        if (typeStr === "patch") {
          let table_ =
            switch (Js.Dict.get(dict, "table")) {
            | Some(t) => (Obj.magic(t): string)
            | None => ""
            };
          let action =
            switch (Js.Dict.get(dict, "action")) {
            | Some(t) => (Obj.magic(t): string)
            | None => ""
            };
          let data =
            switch (Js.Dict.get(dict, "data")) {
            | Some(d) => Some(inventory_item_from_json(d))
            | None => None
            };
          let id =
            switch (Js.Dict.get(dict, "id")) {
            | Some(i) => Some(Obj.magic(i): string)
            | None => None
            };
          Some({
            type_: "patch",
            table_,
            action,
            data,
            id,
          });
        } else {
          None;
        };
      | None => None
      };
    }) {
    | _ => None
    };

  let find_existing_period_list = (currentConfig: Config.t, itemId: string) => {
    switch (Js.Array.find(~f=(i: Config.InventoryItem.t) => i.id === itemId, currentConfig.inventory)) {
    | Some(existingItem) => existingItem.period_list
    | None => [||]
    };
  };

  let apply_patch = (currentConfig: Config.t, patch: patch_message): Config.t => {
    switch (patch.table_, patch.action) {
    | ("inventory", "INSERT" | "UPDATE") =>
      switch (patch.data) {
      | Some(newItem) =>
        let period_list = find_existing_period_list(currentConfig, newItem.id);
        let itemWithPeriod = {...newItem, period_list};
        let exists =
          currentConfig.inventory
          |> Js.Array.some(~f=(i: Config.InventoryItem.t) =>
               i.id === newItem.id
             );
        let newInventory =
          if (exists) {
            currentConfig.inventory
            |> Js.Array.map(~f=(i: Config.InventoryItem.t) =>
                 i.id === itemWithPeriod.id ? itemWithPeriod : i
               );
          } else {
            Array.append(currentConfig.inventory, [|itemWithPeriod|]);
          };
        {
          ...currentConfig,
          inventory: newInventory,
        };
      | None => currentConfig
      }
    | ("inventory", "DELETE") =>
      switch (patch.id) {
      | Some(id) =>
        let newInventory =
          currentConfig.inventory
          |> Js.Array.filter(~f=(i: Config.InventoryItem.t) => i.id !== id);
        {
          ...currentConfig,
          inventory: newInventory,
        };
      | None => currentConfig
      }
    | _ => currentConfig
    };
  };

  let rec subscribe = (set: Config.t => unit, get: unit => Config.t, premise_id: string, updated_at: float) => {
    let url =
      Webapi.Url.makeWith(
        Constants.event_url
        ++ "?premise_id="
        ++ premise_id
        ++ "&ts="
        ++ updated_at->Float.to_string,
        ~base=base_url,
      );
    url->Webapi.Url.setProtocol("ws");

    let pathname = Webapi.Dom.Location.pathname(Webapi.Dom.location);
    let ws = WebSocket.make(url->Webapi.Url.href);
    let path =
      switch (pathname) {
      | "/" => ["/"]
      | _ => pathname |> Js.String.split(~sep="/") |> Array.to_list
      };
    Js.log(path);
    let timeout = 5.0;
    let signal = Tilia.Core.signal;
    let lift = Tilia.Core.lift;
    let (last_pong_ts, set_last_pong_ts) = signal(0.0);
    let (last_ping_ts, set_last_ping_ts) = signal(0.0);
    let (updated_ts, set_updated_ts) = signal(updated_at);
    let state: websocket_state =
      Tilia.Core.make({
        last_ping: last_ping_ts->lift,
        last_pong: last_pong_ts->lift,
        updated_at: updated_ts->lift,
      });
    let send_ping = () =>
      if (ws->WebSocket.readyState == 1) {
        // I don't know if this is the best way to send a ping packet or not.
        ws->WebSocket.send_string("ping");
        set_last_ping_ts(Js.Date.fromString("now")->Js.Date.getTime);
      };
    let select = (premise_id: string) => {
      Js.log("Sending select subscription: " ++ premise_id);
      ws->WebSocket.send_string("select " ++ premise_id);
    };

    Tilia.Core.observe(() => {
      let elapsed = state.last_pong -. state.last_ping;
      if (elapsed > timeout) {
        Js.log("No pong received from server, reconnecting...");
        ws->WebSocket.close;
        subscribe(set, get, premise_id, Js.Date.now());
      };
    });
    switch (globalThis->Js.Dict.get("interval")) {
    | Some(_) => ()
    | None =>
      setInterval(. () => send_ping(), Float.to_int(timeout) * 1000)
      |> globalThis->Js.Dict.set("interval")
    };
    WebSocket.onOpen(ws, () => {select(premise_id)});
    WebSocket.onClose(
      ws,
      () => {
        Js.log("WebSocket closed, reconnecting");
        subscribe(set, get, premise_id, state.updated_at);
      },
    );
    WebSocket.onMessage(
      ws,
      event => {
        let data: string = event##data;
        if (data === "pong") {
          set_last_pong_ts(Js.Date.fromString("now")->Js.Date.getTime);
        } else {
          let json = Js.Json.parseExn(data);
          switch (parse_patch(json)) {
          | Some(patch) =>
            Js.log(
              "Received patch: " ++ patch.action ++ " on " ++ patch.table_,
            );
            let currentConfig = get();
            let newConfig = apply_patch(currentConfig, patch);
            set(newConfig);
          | None =>
            let config: Config.t = Js.Json.deserializeUnsafe(data);
            switch (config.premise) {
            | Some(premise) => set_updated_ts(Obj.magic(premise.updated_at))
            | _ => set_updated_ts(Js.Date.now())
            };
            set(config);
          };
        };
      },
    );
  };
};
