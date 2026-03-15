/* PgNotifier.re - Dedicated PostgreSQL connection for LISTEN/NOTIFY using Postgresql library */
open Lwt.Syntax;

/* Dedicated PostgreSQL connection for notifications */
let pg_conn = ref(None);

/* Notification type for handler dispatch (migrated from PgListener) */
type notification = {
  channel: string,
  payload: string,
};

type handler = notification => Lwt.t(unit);

/* Map of channel name to list of handlers */
let handlers = Hashtbl.create(100);

/* Register a handler for a channel */
let register_handler = (channel: string, handler: handler) => {
  let current =
    switch (Hashtbl.find_opt(handlers, channel)) {
    | Some(hs) => hs
    | None => []
    };
  Hashtbl.replace(handlers, channel, [handler, ...current]);
  Lwt.return();
};

/* Unregister a handler from a channel */
let unregister_handler = (channel: string, handler: handler) => {
  switch (Hashtbl.find_opt(handlers, channel)) {
  | Some(hs) =>
    let filtered = List.filter(h => h != handler, hs);
    Hashtbl.replace(handlers, channel, filtered);
    Lwt.return();
  | None => Lwt.return()
  };
};

/* Dispatch a notification to all handlers of a channel */
let dispatch_notification = (channel: string, payload: string) => {
  print_endline("[PgNotifier] Dispatching notification on channel: " ++ channel);
  switch (Hashtbl.find_opt(handlers, channel)) {
  | Some(hs) =>
    print_endline("[PgNotifier] Broadcasting to " ++ string_of_int(List.length(hs)) ++ " handler(s)");
    Lwt_list.iter_p(
      handler =>
        handler({
          channel,
          payload,
        }),
      hs,
    )
  | None =>
    print_endline("[PgNotifier] No handlers registered for channel: " ++ channel);
    Lwt.return()
  };
};

/* Parse a PostgreSQL URI to extract connection parameters */
let parse_uri = (uri_str: string) => {
  let uri = Uri.of_string(uri_str);
  let host = switch (Uri.host(uri)) { | Some(h) => h | None => "127.0.0.1" };
  let port = switch (Uri.port(uri)) { | Some(p) => string_of_int(p) | None => "5432" };
  let (user, password) = switch (Uri.userinfo(uri)) {
    | Some(userinfo_str) =>
        /* Parse "user:password" format */
        switch (String.split_on_char(':', userinfo_str)) {
        | [u] => (u, "")
        | [u, p, ..._] => (u, p)
        | [] => ("", "")
        }
    | None => ("", "")
  };
  let path = Uri.path(uri);
  let dbname =
    if (String.length(path) > 1 && path.[0] == '/') {
      String.sub(path, 1, String.length(path) - 1);
    } else {
      path;
    };
  (host, port, user, password, dbname);
};

/* Create a dedicated connection for notifications */
let connect = (uri_str: string) => {
  switch (pg_conn^) {
  | Some(_) =>
    print_endline("[PgNotifier] Already connected");
    Lwt.return_ok();
  | None =>
    print_endline("[PgNotifier] Creating dedicated PostgreSQL connection");
    let (host, port, user, password, dbname) = parse_uri(uri_str);
    let connect_result =
      try {
        let conn = new Postgresql.connection(
          ~host,
          ~port,
          ~dbname,
          ~user,
          ~password,
          ()
        );
        Ok(conn);
      } {
      | Postgresql.Error(err) =>
        let msg = Postgresql.string_of_error(err);
        Error(msg)
      };
    switch (connect_result) {
    | Ok(conn) =>
      let conn_status = conn##status;
      if (conn_status == Postgresql.Ok) {
        pg_conn := Some(conn);
        print_endline("[PgNotifier] Connected successfully");
        Lwt.return_ok();
      } else {
        let msg = conn##error_message;
        print_endline("[PgNotifier] Connection status not OK: " ++ msg);
        Lwt.return_error(`Msg(msg));
      }
    | Error(msg) =>
      print_endline("[PgNotifier] Connection failed: " ++ msg);
      Lwt.return_error(`Msg(msg));
    };
  };
};

/* Send LISTEN command on the dedicated connection */
let listen = (channel: string) => {
  switch (pg_conn^) {
  | None =>
    print_endline("[PgNotifier] No connection available");
    Lwt.return_unit
  | Some(conn) =>
    try {
      let query = "LISTEN \"" ++ channel ++ "\"";
      print_endline("[PgNotifier] Executing: " ++ query);
      let result = conn##exec(query);
      let status = result##status;
      if (status == Postgresql.Command_ok) {
        print_endline("[PgNotifier] LISTEN sent for channel: " ++ channel);
      } else {
        print_endline("[PgNotifier] LISTEN failed with status: " ++ Postgresql.result_status(status));
        let errmsg = result##error;
        print_endline("[PgNotifier] Error: " ++ errmsg);
      };
      Lwt.return_unit;
    } {
    | Postgresql.Error(err) =>
      let msg = Postgresql.string_of_error(err);
      print_endline("[PgNotifier] LISTEN failed: " ++ msg);
      Lwt.return_unit;
    };
  };
};

/* Poll for notifications and dispatch to handlers */
let rec poll = () => {
  switch (pg_conn^) {
  | None =>
    print_endline("[PgNotifier] No connection, stopping poll");
    Lwt.return();
  | Some(conn) =>
    Lwt.bind(
      Lwt_unix.sleep(0.1),
      () => {
        try {
          conn##consume_input;
          let rec process_notifications = () => {
            switch (conn##notifies) {
            | None => ()
            | Some(notif) =>
              let channel = notif.name;
              let payload = notif.extra;
              print_endline("[PgNotifier] Received notification on channel: " ++ channel);
              print_endline("[PgNotifier] Payload: " ++ payload);
              Lwt.async(() => dispatch_notification(channel, payload));
              process_notifications()
            };
          };
          process_notifications();
        } {
        | Postgresql.Error(err) =>
          let msg = Postgresql.string_of_error(err);
          print_endline("[PgNotifier] Poll error: " ++ msg)
        };
        poll()
      }
    )
  };
};

/* Start polling for notifications in the background */
let start = () => {
  print_endline("[PgNotifier] Starting notification polling");
  Lwt.async(poll);
};