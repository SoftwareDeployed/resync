let json_text decode =
  Caqti_type.custom
    ~encode:(fun _ -> Error "encoding JSON text is not supported")
    ~decode:(fun json_text ->
      try Ok (decode (Melange_json.of_string json_text)) with
      | exn -> Error (Printexc.to_string exn))
    Caqti_type.string
