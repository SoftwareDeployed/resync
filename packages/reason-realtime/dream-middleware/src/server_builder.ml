type t = {
  doc_root : string;
  db_uri : string option;
  interface : string;
  port : int;
  adapter : Adapter.packed option;
  middleware : Dream.route option;
  extra_routes : Dream.route list;
  pre_start : (unit -> unit) list;
}

let getenv ~var ~default =
  match Sys.getenv_opt var with
  | Some value -> value
  | None -> default

let getenv_required var =
  match Sys.getenv_opt var with
  | Some value -> value
  | None -> failwith (var ^ " is required")

let make
    ?doc_root
    ?(doc_root_var = "DOC_ROOT")
    ?db_uri
    ?db_url_var
    ?interface
    ?(interface_var = "SERVER_INTERFACE")
    ?port
    ?(port_var = "SERVER_PORT")
    ?default_interface
    ?default_port
    () =
  let doc_root = match doc_root with Some d -> d | None -> getenv_required doc_root_var in
  let db_uri =
    match db_uri with
    | Some d -> Some d
    | None -> match db_url_var with Some v -> Some (getenv_required v) | None -> None
  in
  let interface = match interface with Some i -> i | None -> match default_interface with Some i -> i | None -> "127.0.0.1" in
  let port = match port with Some p -> p | None -> match default_port with Some p -> p | None -> 8080 in
  let interface = getenv ~var:interface_var ~default:interface in
  let port =
    match int_of_string_opt (getenv ~var:port_var ~default:(string_of_int port)) with
    | Some p -> p
    | None -> port
  in
  {
    doc_root;
    db_uri;
    interface;
    port;
    adapter = None;
    middleware = None;
    extra_routes = [];
    pre_start = [];
  }

let doc_root t = t.doc_root

let db_uri t = t.db_uri

let with_packed_adapter adapter t = { t with adapter = Some adapter }


let with_middleware
    ~resolve_subscription
    ~load_snapshot
    ?handle_mutation
    ?validate_mutation
    ?handle_media
    ?handle_disconnect
    t =
  match t.adapter with
  | None -> failwith "Server_builder.with_middleware requires an adapter"
  | Some adapter ->
      let middleware =
        Middleware.create ~adapter ~resolve_subscription ~load_snapshot
          ?handle_mutation ?validate_mutation ?handle_media ?handle_disconnect ()
      in
      { t with middleware = Some (Middleware.route "_events" middleware) }


let with_routes routes t = { t with extra_routes = t.extra_routes @ routes }

let with_pre_start f t = { t with pre_start = f :: t.pre_start }

let run t =
  (match t.adapter with
   | Some adapter ->
       (match Lwt_main.run (Adapter.start adapter) with
        | () -> ()
        | exception Failure msg ->
            Printf.eprintf "Failed to connect notification listener: %s\n%!" msg)
   | None -> ());
  List.iter (fun f -> f ()) (List.rev t.pre_start);
  let middleware_routes = match t.middleware with Some m -> [ m ] | None -> [] in
  let all_routes = middleware_routes @ t.extra_routes in
  let app = Dream.router all_routes in
  let with_pool =
    match t.db_uri with
    | Some db_uri -> Dream.sql_pool ~size:10 db_uri @@ app
    | None -> app
  in
  Dream.run ~interface:t.interface ~port:t.port @@ Dream.logger @@ with_pool
