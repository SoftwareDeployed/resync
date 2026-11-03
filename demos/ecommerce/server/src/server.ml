open Lwt.Syntax

let get_config request premise_id =
  let* premise_row =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetPremise.find_opt
        db
        RealtimeSchema.Queries.GetPremise.caqti_type
        premise_id)
  in
  let premise =
    Option.map
      (fun (row : RealtimeSchema.Queries.GetPremise.row) ->
         ({ PeriodList.Premise.id = row.id; name = row.name; description = row.description;
            updated_at = Js.Date.fromFloat row.updated_at } : PeriodList.Premise.t))
      premise_row
  in
  let* inventory_rows =
    Dream.sql request (fun db ->
      RealtimeSchema.Queries.GetInventoryList.collect
        db
        RealtimeSchema.Queries.GetInventoryList.caqti_type
        premise_id)
  in
  let inventory =
    Array.map
      (fun (row : RealtimeSchema.Queries.GetInventoryList.row) ->
         ({ Model.InventoryItem.description = row.description; id = row.id; name = row.name;
            quantity = row.quantity; premise_id = row.premise_id;
            period_list = Model.Pricing.period_list_of_json (Melange_json.of_string row.period_list) }
           : Model.InventoryItem.t))
      (Array.of_list inventory_rows)
  in
  let config : Model.t = {inventory; premise; order = None} in
  Lwt.return config

let get_config_json request premise_id =
  let* config = get_config request premise_id in
  Lwt.return (Store.serializeSnapshot config)

let resolve_subscription request selection =
  match RealtimeSubscription.decode_channel selection with
  | None -> Lwt.return_none
  | Some premise_id ->
      let* premise_row =
        Dream.sql request (fun db ->
          RealtimeSchema.Queries.GetPremise.find_opt
            db
            RealtimeSchema.Queries.GetPremise.caqti_type
            premise_id)
      in
      Lwt.return (Option.map (fun _ -> premise_id) premise_row)

let form_value fields name =
  List.find_opt (fun (key, _) -> key = name) fields |> Option.map snd

(* Cart lines arrive as item_<inventory_id>=<quantity> fields. *)
let form_items fields =
  List.filter_map
    (fun (key, value) ->
       if String.length key > 5 && String.sub key 0 5 = "item_" then
         let inventory_id = String.sub key 5 (String.length key - 5) in
         match int_of_string_opt value with
         | Some quantity when quantity > 0 && StoreChannel.isValidUuid inventory_id ->
             Some (inventory_id, quantity)
         | _ -> None
       else None)
    fields

let bad_request message =
  Dream.html ~status:`Bad_Request
    (Printf.sprintf "<!DOCTYPE html><html><body><h1>Checkout failed</h1><p>%s</p><p><a href='/cart'>Back to cart</a></p></body></html>"
       message)

let checkout_handler request =
  let* form = Dream.form ~csrf:false request in
  match form with
  | `Ok fields ->
    let field name = form_value fields name in
    let required name =
      match field name with
      | Some value when value <> "" -> value
      | _ -> ""
    in
    let premise_id = required "premise_id"
    and customer_name = required "customer_name"
    and customer_email = required "customer_email"
    and start_date = required "start"
    and end_date = required "end"
    and unit_label = field "unit" in
    let items = form_items fields in
    (if premise_id = "" || customer_name = "" || customer_email = "" || start_date = ""
       || end_date = "" || items = [] then
      bad_request "Missing customer details, reservation dates, or cart items."
    else if not (StoreChannel.isValidUuid premise_id) then
      bad_request "Unknown premise."
    else
      let unit = match unit_label with Some unit -> unit | None -> "month" in
      (* Verify stock for every line before creating anything. *)
      let* stock_errors =
        Lwt_list.filter_map_s
          (fun (inventory_id, quantity) ->
             let* item_row =
               Dream.sql request (fun db ->
                 RealtimeSchema.Queries.GetCompleteInventory.find_opt
                   db
                   RealtimeSchema.Queries.GetCompleteInventory.caqti_type
                   inventory_id)
             in
             match item_row with
             | Some (row : RealtimeSchema.Queries.GetCompleteInventory.row)
               when row.quantity >= quantity -> Lwt.return_none
             | Some (row : RealtimeSchema.Queries.GetCompleteInventory.row) ->
                 Lwt.return
                   (Some
                      (Printf.sprintf "Only %d of %s left in stock."
                         row.quantity row.name))
             | None -> Lwt.return (Some "An item in your cart no longer exists."))
          items
      in
      match stock_errors with
      | error :: _ -> bad_request error
      | [] ->
          let order_id = UUID.make () in
          let* () =
            Dream.sql request (fun db ->
              RealtimeSchema.Mutations.CreateOrder.exec
                db
                (order_id, premise_id, customer_name, customer_email, start_date, end_date))
          in
          let* () =
            Lwt_list.iter_s
              (fun (inventory_id, quantity) ->
                 let* period_row =
                   Dream.sql request (fun db ->
                     RealtimeSchema.Queries.GetPeriodForUnit.find_opt
                       db
                       RealtimeSchema.Queries.GetPeriodForUnit.caqti_type
                       (inventory_id, unit))
                 in
                 let period_label, period_price =
                   match period_row with
                   | Some (row : RealtimeSchema.Queries.GetPeriodForUnit.row) ->
                       (row.label, row.price)
                   | None -> ("Rental", 0L)
                 in
                 let* () =
                   Dream.sql request (fun db ->
                     RealtimeSchema.Mutations.AddOrderItem.exec
                       db
                       (order_id, inventory_id, quantity, period_label, period_price))
                 in
                 Dream.sql request (fun db ->
                   RealtimeSchema.Mutations.DecrementInventory.exec
                     db
                     (inventory_id, quantity)))
              items
          in
          Dream.redirect ~status:`See_Other request ("/order/" ^ order_id))
  | _ -> bad_request "Invalid form submission."

let () =
  let builder =
    Server_builder.make
      ~doc_root_var:"ECOMMERCE_DOC_ROOT"
      ~db_url_var:"DB_URL"
      ~default_interface:"127.0.0.1"
      ~default_port:8899
      ()
  in
  let doc_root = Server_builder.doc_root builder in
  let db_uri = Option.get (Server_builder.db_uri builder) in
  let adapter =
    Adapter.pack
      (module Pgnotify_adapter : Adapter.S with type t = Pgnotify_adapter.t)
      (Pgnotify_adapter.create ~db_uri ())
  in
  builder
  |> Server_builder.with_packed_adapter adapter
  |> Server_builder.with_middleware
    ~resolve_subscription
    ~load_snapshot:get_config_json
  |> Server_builder.with_routes (
       Server_builder.ui_routes ~doc_root
       @ [
           Dream.get "/api/test/inventory/:id/quantity/:quantity" (fun req ->
             let item_id = Dream.param req "id" in
             let quantity = Dream.param req "quantity" |> int_of_string in
             let* () = Dream.sql req (fun db ->
               RealtimeSchema.Mutations.UpdateQuantity.exec db (item_id, quantity)) in
             Dream.respond ~status:`OK "updated");
           Dream.post "/api/checkout" checkout_handler;
           Dream.get "/" (UniversalRouterDream.handler ~app:EntryServer.app);
           Dream.get "/**" (UniversalRouterDream.handler ~app:EntryServer.app);
         ]
     )
  |> Server_builder.run
