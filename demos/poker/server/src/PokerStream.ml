open Lwt.Syntax
open PokerTypes

let local_delay = 0.620
let next_hand_delay = 2.8
let jev_thinking_minimum = 1.5

let handler ~configured_model ~configured_key ~upstream_decision request =
  let origin = Dream.header request "origin" in
  let host = Dream.header request "host" in
  if not (PokerProtocol.origin_allowed ~origin ~host) then
    Dream.respond ~status:`Forbidden "WebSocket origin is not allowed"
  else
    Dream.websocket (fun socket ->
      let game = ref (PokerEngine.createGame ~seed:(Random.int 1_073_741_823) ()) in
      let opponent_mode =
        ref (match configured_key () with Some _ -> PokerProtocol.Jev | None -> PokerProtocol.Local)
      in
      let closed = ref false in
      let decision_in_flight = ref false in
      let next_hand_in_flight = ref false in
      let epoch = ref 0 in
      let send_tail = ref Lwt.return_unit in
      let send_text text =
        if !closed then Lwt.return_unit
        else
          let operation =
            Lwt.catch
              (fun () ->
                 let* () = !send_tail in
                 if !closed then Lwt.return_unit else Dream.send socket text)
              (fun _exn -> Lwt.return_unit)
          in
          send_tail := operation;
          operation
      in
      let enqueue text = Lwt.ignore_result (send_text text) in
      let send_error message = enqueue (PokerProtocol.error_message message) in
      let send_status () =
        enqueue
          (PokerProtocol.status_message
             ~available:(configured_key () <> None)
             ~model:(configured_model ())
             !opponent_mode)
      in
      let invalidate_work () =
        incr epoch;
        decision_in_flight := false;
        next_hand_in_flight := false
      in
      let rec drive () =
        if (not !closed) && not !decision_in_flight then
          let current = !game in
          if current.handOver then schedule_next_hand ()
          else
            match current.currentPlayer with
            | Some index when index <> PokerEngine.humanIndex ->
                (match !opponent_mode with
                 | PokerProtocol.Local -> schedule_local ~delay:local_delay ()
                 | PokerProtocol.Jev -> schedule_jev ())
            | _ -> ()
      and commit next =
        invalidate_work ();
        game := next;
        enqueue (PokerProtocol.state_message next);
        drive ()
      and schedule_local ?(fallback_reason = "") ~delay () =
        let before = !game in
        match before.currentPlayer with
        | Some index when index <> PokerEngine.humanIndex && not before.handOver ->
            let token = !epoch in
            let hand_number = before.handNumber in
            let snapshot = PokerProtocol.native_snapshot before in
            decision_in_flight := true;
            if fallback_reason <> "" then
              send_error
                ("Jev fallback | " ^ before.players.(index).name
               ^ " used local rules (" ^ fallback_reason ^ ")");
            Lwt.async (fun () ->
                let* () = if delay > 0.0 then Lwt_unix.sleep delay else Lwt.return_unit in
                if
                  (not !closed) && !epoch = token
                  && (!game).handNumber = hand_number
                  && (not (!game).handOver)
                  && (!game).currentPlayer = Some index
                  && PokerProtocol.native_snapshot !game = snapshot
                then
                  let current = !game in
                  let action = PokerComputer.chooseAction current index in
                  match PokerEngine.applyAction current index action with
                  | Ok next -> commit next; Lwt.return_unit
                  | Error message -> decision_in_flight := false; send_error message; Lwt.return_unit
                else Lwt.return_unit)
        | _ -> decision_in_flight := false
      and schedule_jev () =
        let before = !game in
        match before.currentPlayer with
        | Some index when index <> PokerEngine.humanIndex && not before.handOver ->
            let token = !epoch in
            let hand_number = before.handNumber in
            let street = before.street in
            let snapshot = PokerProtocol.native_snapshot before in
            let request = PokerProtocol.decision_request before index in
            let model = configured_model () in
            decision_in_flight := true;
            Lwt.async (fun () ->
                let* () =
                  send_text
                    (PokerProtocol.thinking_message
                       ~seat:index ~hand_number ~street ~model)
                in
                if !closed || !epoch <> token then Lwt.return_unit
                else
                  let thinking_started = Unix.gettimeofday () in
                  match configured_key () with
                  | None ->
                      schedule_local
                        ~fallback_reason:"Jev is not configured"
                        ~delay:jev_thinking_minimum ()
                      ;
                      Lwt.return_unit
                  | Some key ->
                      let upstream_body = JevDecision.upstream_body ~model request in
                      let* response = upstream_decision ~key upstream_body in
                      let elapsed = Unix.gettimeofday () -. thinking_started in
                      let remaining = max 0.0 (jev_thinking_minimum -. elapsed) in
                      let* () = if remaining > 0.0 then Lwt_unix.sleep remaining else Lwt.return_unit in
                      if
                        (not !closed) && !epoch = token
                        && (!game).handNumber = hand_number
                        && (not (!game).handOver)
                        && (!game).currentPlayer = Some index
                        && PokerProtocol.native_snapshot !game = snapshot
                      then
                        match response with
                        | Error `Timeout ->
                            schedule_local ~fallback_reason:"Jev timed out" ~delay:0.0 ();
                            Lwt.return_unit
                        | Error `Upstream ->
                            schedule_local ~fallback_reason:"Jev is unavailable" ~delay:0.0 ();
                            Lwt.return_unit
                        | Ok response_body ->
                            (match JevDecision.parse_response ~request response_body with
                             | Error message ->
                                 schedule_local ~fallback_reason:message ~delay:0.0 ();
                                 Lwt.return_unit
                             | Ok decision ->
                                 (match PokerProtocol.action_of_decision decision with
                                  | Error message ->
                                      schedule_local ~fallback_reason:message ~delay:0.0 ();
                                      Lwt.return_unit
                                  | Ok action ->
                                      (match PokerEngine.applyAction before index action with
                                       | Error message ->
                                           schedule_local ~fallback_reason:message ~delay:0.0 ();
                                           Lwt.return_unit
                                       | Ok next ->
                                           let action_label = next.players.(index).lastAction in
                                           invalidate_work ();
                                           game := next;
                                           enqueue
                                             (PokerProtocol.jev_decision_message
                                                ~seat:index ~hand_number ~street ~action_label decision);
                                           enqueue (PokerProtocol.state_message next);
                                           drive ();
                                           Lwt.return_unit)))
                      else Lwt.return_unit)
        | _ -> decision_in_flight := false
      and schedule_next_hand () =
        let before = !game in
        if
          (not !next_hand_in_flight) && before.handOver
          && before.players.(PokerEngine.humanIndex).chips > 0
          && PokerEngine.fundedCount before >= 2
        then begin
          let token = !epoch in
          let hand_number = before.handNumber in
          let snapshot = PokerProtocol.native_snapshot before in
          next_hand_in_flight := true;
          Lwt.async (fun () ->
              let* () = Lwt_unix.sleep next_hand_delay in
              if
                (not !closed) && !epoch = token && !next_hand_in_flight
                && (!game).handNumber = hand_number && (!game).handOver
                && (!game).players.(PokerEngine.humanIndex).chips > 0
                && PokerEngine.fundedCount !game >= 2
                && PokerProtocol.native_snapshot !game = snapshot
              then begin
                next_hand_in_flight := false;
                match PokerEngine.startHand before with
                | Ok next -> commit next; Lwt.return_unit
                | Error message -> send_error message; Lwt.return_unit
              end
              else Lwt.return_unit)
        end
      in
      let handle_intent = function
        | PokerProtocol.Deal ->
            (match PokerEngine.startHand !game with
             | Ok next -> commit next
             | Error message -> send_error message)
        | PokerProtocol.Restart ->
            let next = PokerEngine.restart ~seed:((!game).rngSeed + 97) () in
            commit next
        | PokerProtocol.Set_mode mode ->
            if mode <> !opponent_mode then begin
              opponent_mode := mode;
              invalidate_work ();
              send_status ();
              drive ()
            end
        | PokerProtocol.Act action ->
            (match PokerEngine.applyAction !game PokerEngine.humanIndex action with
             | Ok next -> commit next
             | Error message -> send_error message)
      in
      let rec receive_loop () =
        let* incoming =
          Lwt.catch
            (fun () -> Dream.receive socket)
            (fun _exn -> Lwt.return_none)
        in
        match incoming with
        | None ->
            closed := true;
            invalidate_work ();
            Lwt.return_unit
        | Some body ->
            (match PokerProtocol.parse_intent body with
             | Ok intent -> handle_intent intent
             | Error message -> send_error message);
            if !closed then Lwt.return_unit else receive_loop ()
      in
      let* () =
        send_text
          (PokerProtocol.status_message
             ~available:(configured_key () <> None)
             ~model:(configured_model ())
             !opponent_mode)
      in
      let* () = send_text (PokerProtocol.state_message !game) in
      drive ();
      receive_loop ())
