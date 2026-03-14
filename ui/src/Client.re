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

  let rec subscribe = (set, premise_id: string, updated_at: float) => {
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
      ws->WebSocket.send_string("select " ++ premise_id);
    };

    Tilia.Core.observe(() => {
      let elapsed = state.last_pong -. state.last_ping;
      if (elapsed > timeout) {
        Js.log("No pong received from server, reconnecting...");
        ws->WebSocket.close;
        set->subscribe(premise_id, Js.Date.now());
      };
    });
    switch (globalThis->Js.Dict.get("interval")) {
    | Some(_) => ()
    | None =>
      setInterval(. () => send_ping(), Float.to_int(timeout) * 1000)
      |> globalThis->Js.Dict.set("interval")
    };
    WebSocket.addEventListener(. ws, WebSocket.Open, _event => {
      select(premise_id)
    });
    WebSocket.addEventListener(.
      ws,
      WebSocket.Close,
      _event => {
        Js.log("WebSocket closed, reconnecting");
        subscribe(set, premise_id, state.updated_at);
      },
    );
    WebSocket.addEventListener(.
      ws,
      WebSocket.Message,
      event => {
        let data: string = event#data;
        if (data === "pong") {
          set_last_pong_ts(Js.Date.fromString("now")->Js.Date.getTime);
        } else {
          let config: Config.t = Js.Json.deserializeUnsafe(data);
          switch (config.premise) {
          | Some(premise) =>
            set_updated_ts(premise.updated_at->Js.Date.getTime)
          | _ => set_updated_ts(Js.Date.now())
          };
          set(config);
        };
      },
    );
  };
};
