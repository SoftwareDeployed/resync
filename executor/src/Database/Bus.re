/* Bus.re - Simple pub/sub for WebSocket updates with PostgreSQL LISTEN/NOTIFY */
open Lwt.Syntax;

/* Map from premise_id to list of WebSocket connections */
let subscriptions = Hashtbl.create(100);

/* Track which premises we're already listening to via PostgreSQL */
let listening_channels = Hashtbl.create(100);

/* Parse notification payload */
type notification_payload = {
  table_: string,
  id: Yojson.Safe.t,
  action: string,
  data: option(Yojson.Safe.t),
};

let parse_payload = (payload: string) => {
  let json = Yojson.Safe.from_string(payload);
  let table_ = json |> Yojson.Safe.Util.member("table") |> Yojson.Safe.Util.to_string;
  let id = json |> Yojson.Safe.Util.member("id");
  let action = json |> Yojson.Safe.Util.member("action") |> Yojson.Safe.Util.to_string;
  let data = 
    try(Some(json |> Yojson.Safe.Util.member("data"))) {
    | _ => None
    };
  {table_, id, action, data};
};

/* Extract string ID from JSON (handles both string and compound keys) */
let get_id_string = (id_json: Yojson.Safe.t) => {
  switch (id_json) {
  | `String(s) => s
  | `Int(i) => Int.to_string(i)
  | `Assoc(fields) =>
      switch (List.assoc_opt("inventory_id", fields)) {
      | Some(`String(s)) => s
      | _ => ""
      }
  | _ => ""
  };
};

/* Broadcast a message to all subscribers of a premise */
let broadcast = (premise_id, message) =>
  switch (Hashtbl.find_opt(subscriptions, premise_id)) {
  | Some(ws_list) =>
    print_endline("[Bus] Broadcasting to " ++ string_of_int(List.length(ws_list)) ++ " WebSocket(s) for premise: " ++ premise_id);
    Lwt_list.iter_p(ws => Dream.send(ws, message), ws_list)
  | None =>
    print_endline("[Bus] No WebSocket subscribers for premise: " ++ premise_id);
    Lwt.return()
  };

/* Handler for PostgreSQL notifications - broadcasts to all WebSockets */
let pg_notification_handler = (notif: PgNotifier.notification) => {
  print_endline("[Bus] Received PostgreSQL notification on channel: " ++ notif.channel);
  print_endline("[Bus] Payload: " ++ notif.payload);
  
  let payload = parse_payload(notif.payload);
  print_endline("[Bus] Parsed - table: " ++ payload.table_ ++ ", action: " ++ payload.action);
  
  switch (payload.table_, payload.action) {
  | ("inventory", ("INSERT" | "UPDATE")) =>
      let patch = `Assoc([
        ("type", `String("patch")),
        ("table", `String("inventory")),
        ("action", `String(payload.action)),
        ("data", switch (payload.data) {
          | Some(d) => d
          | None => `Null
        }),
      ]);
      let patch_json = Yojson.Safe.to_string(patch);
      broadcast(notif.channel, patch_json)
  | ("inventory", "DELETE") =>
      let patch = `Assoc([
        ("type", `String("patch")),
        ("table", `String("inventory")),
        ("action", `String("DELETE")),
        ("id", payload.id),
      ]);
      let patch_json = Yojson.Safe.to_string(patch);
      broadcast(notif.channel, patch_json)
  | ("premise", ("INSERT" | "UPDATE")) =>
      Lwt.return()
  | _ =>
      print_endline("[Bus] Ignoring notification for table: " ++ payload.table_);
      Lwt.return()
  };
};

/* Handle the result of sending LISTEN command */
let handle_listen_result = (premise_id, listen_result) => {
  switch (listen_result) {
  | Ok () => {
    /* Register handler for this channel */
    print_endline("[Bus] Successfully sent LISTEN command for premise: " ++ premise_id);
    PgNotifier.register_handler(premise_id, pg_notification_handler)
  }
  | Error(err) => {
    /* Failed to listen, unmark and return */
    print_endline("[Bus] ERROR: Failed to send LISTEN command for premise: " ++ premise_id ++ " - " ++ Caqti_error.show(err));
    Hashtbl.remove(listening_channels, premise_id);
    Lwt.return();
  }
  };
};

/* Subscribe a WebSocket to updates for a premise and send initial config */
let subscribe_and_send_config = (request, get_config, premise_id, websocket) => {
  let current =
    switch (Hashtbl.find_opt(subscriptions, premise_id)) {
    | Some(ws_list) => ws_list
    | None => []
    };
  Hashtbl.replace(subscriptions, premise_id, [websocket, ...current]);

  /* If not already listening to this premise, set up PostgreSQL LISTEN */
  let* () =
    switch (Hashtbl.find_opt(listening_channels, premise_id)) {
    | Some(_) => {
        print_endline("[Bus] Already listening to premise: " ++ premise_id);
        Lwt.return() /* Already listening */
      }
    | None => {
        print_endline("[Bus] Setting up LISTEN for premise: " ++ premise_id);
        /* Mark as listening */
        Hashtbl.replace(listening_channels, premise_id, true);
        /* Send LISTEN command using the dedicated notifier connection */
        let* () = PgNotifier.listen(premise_id);
        /* Register handler for this channel */
        print_endline("[Bus] Successfully set up LISTEN for premise: " ++ premise_id);
        PgNotifier.register_handler(premise_id, pg_notification_handler)
      }
    };

  /* Fetch and send the initial config */
  let* config = get_config(request, premise_id);
  let config_json = Config.to_yojson(config)->Yojson.Safe.to_string;
  Dream.send(websocket, config_json);
};

/* Unsubscribe a WebSocket from updates */
let unsubscribe = (premise_id, websocket) => {
  switch (Hashtbl.find_opt(subscriptions, premise_id)) {
  | Some(ws_list) =>
    let filtered = List.filter(ws => ws != websocket, ws_list);
    Hashtbl.replace(subscriptions, premise_id, filtered);
    Lwt.return();
  | None => Lwt.return()
  };
};

/* Handle WebSocket message */
let handle_message = (request, get_config, websocket, msg) => {
  print_endline("[Bus] Received WebSocket message: " ++ msg);
  let tokens = String.split_on_char(' ', msg);
  switch (tokens) {
  | ["ping"] =>
      print_endline("[Bus] Handling ping command");
      Dream.send(websocket, "pong")
  | ["select", premise_id] =>
      print_endline("[Bus] Handling select command for premise: " ++ premise_id);
      subscribe_and_send_config(request, get_config, premise_id, websocket)
  | _ =>
      print_endline("[Bus] Unknown command received");
      Dream.send(websocket, "Unknown command")
  };
};
