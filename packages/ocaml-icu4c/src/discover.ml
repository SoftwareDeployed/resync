module C = Configurator.V1

let has_prefix str prefix =
  let lp = String.length prefix in
  let ls = String.length str in
  ls >= lp && String.sub str 0 lp = prefix

let add_unique_flag existing flag =
  if List.mem flag existing then existing else existing @ [ flag ]

let has_lib path =
  let lib = Filename.concat path "lib" in
  (* Check for version-agnostic library names (symlinks maintained by Homebrew) *)
  let checks =
    [ "libicuuc.a"; "libicuuc.so"; "libicuuc.dylib"
    ; "libicui18n.a"; "libicui18n.so"; "libicui18n.dylib" ] in
  List.exists (fun name -> Sys.file_exists (Filename.concat lib name)) checks

let has_headers path =
  Sys.file_exists (Filename.concat path "include/unicode/utypes.h")

(* Dynamically discover ICU installations from Homebrew Cellar directories *)
let discover_icu_cellar_paths cellar_root =
  let icu_paths = ref [] in
  (try
     let cellars = Sys.readdir cellar_root in
     Array.iter (fun dir ->
       if has_prefix dir "icu4c@" then
         let icu_base = Filename.concat cellar_root dir in
         (try
            let versions = Sys.readdir icu_base in
            Array.iter (fun version ->
              let full_path = Filename.concat icu_base version in
              if Sys.is_directory full_path then
                icu_paths := full_path :: !icu_paths
            ) versions
          with Sys_error _ -> ())
     ) cellars
   with Sys_error _ -> ());
  List.sort_uniq String.compare !icu_paths

(* Build a list of ICU candidate prefixes dynamically *)
let get_icu_candidate_prefixes () =
  let base_candidates =
    [ "/usr"
    ; "/usr/local"
    ; "/opt/homebrew/opt/icu4c"  (* Homebrew symlink to latest *)
    ; "/usr/local/opt/icu4c"     (* Intel Mac Homebrew symlink *)
    ] in
  let cellar_paths =
    List.concat
      [ discover_icu_cellar_paths "/opt/homebrew/Cellar"
      ; discover_icu_cellar_paths "/usr/local/Cellar"
      ; discover_icu_cellar_paths "/home/linuxbrew/.linuxbrew/Cellar"
      ] in
  (* Add opt paths for specific versions found in cellar *)
  let opt_paths =
    List.filter_map (fun cellar_path ->
      if has_prefix cellar_path "/opt/homebrew/Cellar/icu4c@" then
        let version_suffix =
          String.sub cellar_path (String.length "/opt/homebrew/Cellar/icu4c@")
            (String.length cellar_path - String.length "/opt/homebrew/Cellar/icu4c@") in
        let version =
          try
            let idx = String.index version_suffix '/' in
            String.sub version_suffix 0 idx
          with Not_found -> version_suffix
        in
        Some ("/opt/homebrew/opt/icu4c@" ^ version)
      else if has_prefix cellar_path "/usr/local/Cellar/icu4c@" then
        let version_suffix =
          String.sub cellar_path (String.length "/usr/local/Cellar/icu4c@")
            (String.length cellar_path - String.length "/usr/local/Cellar/icu4c@") in
        let version =
          try
            let idx = String.index version_suffix '/' in
            String.sub version_suffix 0 idx
          with Not_found -> version_suffix
        in
        Some ("/usr/local/opt/icu4c@" ^ version)
      else
        None
    ) cellar_paths
  in
  let all_candidates = base_candidates @ opt_paths @ cellar_paths in
  (* Remove duplicates and return *)
  List.sort_uniq String.compare all_candidates

(* Get pkg-config paths from discovered ICU installations *)
let get_pkg_config_paths () =
  let base_paths =
    [ "/opt/homebrew/opt/icu4c/lib/pkgconfig"
    ; "/usr/local/opt/icu4c/lib/pkgconfig"
    ; "/usr/lib/pkgconfig"
    ; "/usr/local/lib/pkgconfig"
    ] in
  let cellar_paths =
    List.concat
      [ discover_icu_cellar_paths "/opt/homebrew/Cellar"
      ; discover_icu_cellar_paths "/usr/local/Cellar"
      ; discover_icu_cellar_paths "/home/linuxbrew/.linuxbrew/Cellar"
      ] in
  let pkg_config_paths =
    List.map (fun prefix -> Filename.concat prefix "lib/pkgconfig") cellar_paths in
  List.filter Sys.file_exists (base_paths @ pkg_config_paths)

let normalize_prefix_from_cflags cflags =
  let includes = List.filter (fun flag -> has_prefix flag "-I") cflags in
  match includes with
  | flag :: _ when String.length flag > 2 ->
    (try
       let include_path = String.sub flag 2 (String.length flag - 2) in
       Some (Filename.dirname include_path)
     with _ -> None)
  | _ -> None

let add_missing_icu_uc libs = add_unique_flag libs "-licuuc"

let has_library_path libs =
  List.exists (fun flag -> has_prefix flag "-L") libs

let add_lib_dir_if_missing maybe_prefix libs =
  match maybe_prefix with
  | None -> libs
  | Some prefix ->
    if has_library_path libs then
      libs
    else
      let path = "-L" ^ Filename.concat prefix "lib" in
      path :: libs

let split_tokens line =
  String.split_on_char ' ' line
  |> List.filter (fun s -> String.length s > 0)

let parse_define_value lines key =
  let rec loop = function
    | [] -> None
    | line :: rest ->
      (match split_tokens line with
       | "#define" :: name :: value :: _ when name = key -> Some value
       | _ -> loop rest)
  in
  loop lines

let parse_icu_version_suffix header_path =
  if not (Sys.file_exists header_path) then
    None
  else
    let lines =
      let ch = open_in header_path in
      let rec read acc =
        try
          read (input_line ch :: acc)
        with End_of_file ->
          close_in ch;
          List.rev acc
      in
      read [] in
    match parse_define_value lines "U_ICU_VERSION_SUFFIX" with
    | Some value -> Some value
    | None ->
      (match parse_define_value lines "U_ICU_VERSION_MAJOR_NUM" with
       | Some major when String.length major > 0 -> Some ("_" ^ major)
       | _ -> None)

let infer_version_suffix_from_prefix prefix =
  parse_icu_version_suffix (Filename.concat prefix "include/unicode/uvernum.h")

let infer_version_suffix () =
  let candidates = get_icu_candidate_prefixes () in
  let prefix_candidates =
    match Sys.getenv_opt "ICU_PREFIX" with
    | Some custom -> custom :: candidates
    | None -> candidates in
  prefix_candidates
  |> List.find_map infer_version_suffix_from_prefix

let extract_library_dirs prefix libs =
  let dirs =
    libs
    |> List.filter_map (fun flag ->
         if has_prefix flag "-L" && String.length flag > 2 then
           Some (String.sub flag 2 (String.length flag - 2))
         else
           None)
    |> List.sort_uniq String.compare in
  match prefix with
  | Some icu_prefix ->
    let prefix_dir = Filename.concat icu_prefix "lib" in
    List.sort_uniq String.compare (prefix_dir :: dirs)
  | None -> dirs

let write_string_list oc values =
  output_string oc "[";
  values
  |> List.iteri (fun index value ->
       if index > 0 then output_string oc "; ";
       output_string oc "\"";
       output_string oc (String.escaped value);
       output_string oc "\"");
  output_string oc "]"

let write_config_file version_suffix library_dirs =
  let oc = open_out "icu4c_config.ml" in
  output_string oc "(* Auto-generated by discover. Do not edit manually. *)\n";
  output_string oc "let version_suffix = \"";
  output_string oc version_suffix;
  output_string oc "\"\n";
  output_string oc "let library_dirs = ";
  write_string_list oc library_dirs;
  output_string oc "\n";
  close_out oc

let normalize_flags prefix flags =
  let cflags = flags.C.Pkg_config.cflags in
  let libs =
    flags.C.Pkg_config.libs
    |> add_lib_dir_if_missing prefix
    |> add_missing_icu_uc in
  C.Pkg_config.({ cflags; libs })

let find_prefix_by_fallback () =
  let candidates = get_icu_candidate_prefixes () in
  let prefix_candidates =
    match Sys.getenv_opt "ICU_PREFIX" with
    | Some custom -> custom :: candidates
    | None -> candidates in
  List.find_opt (fun prefix -> has_headers prefix && has_lib prefix) prefix_candidates

let try_pkg_config c =
  let config_paths = get_pkg_config_paths () in
  let current =
    Option.value (Sys.getenv_opt "PKG_CONFIG_PATH") ~default:"" in
  Unix.putenv "PKG_CONFIG_PATH"
    (String.concat ":" (current :: List.filter Sys.file_exists config_paths));
  match C.Pkg_config.get c with
  | None -> None
  | Some pc ->
    (match C.Pkg_config.query pc ~package:"icu-i18n" with
     | None -> None
     | Some deps ->
       let prefix = normalize_prefix_from_cflags deps.C.Pkg_config.cflags in
       Some (prefix, normalize_flags prefix deps))

let () =
  C.main ~name:"icu" (fun c ->
    let prefix, flags =
      match try_pkg_config c with
      | Some (prefix, deps) -> (prefix, deps)
      | None ->
        (match find_prefix_by_fallback () with
         | Some prefix ->
           let lib_dir = Filename.concat prefix "lib" in
           let normalized =
             C.Pkg_config.({
               cflags = ["-I" ^ Filename.concat prefix "include"];
               libs = ["-L" ^ lib_dir; "-licui18n"];
             }) in
           (Some prefix, normalize_flags (Some prefix) normalized)
         | None ->
           failwith "Unable to locate ICU headers and libraries; install ICU (icu4c) or set PKG_CONFIG_PATH") in
    let version_suffix =
      match (match prefix with
      | Some icu_prefix -> infer_version_suffix_from_prefix icu_prefix
      | None -> None) with
      | Some suffix -> suffix
      | None -> infer_version_suffix () |> Option.value ~default:"_78" in
    let library_dirs = extract_library_dirs prefix flags.libs in
    write_config_file version_suffix library_dirs;
    C.Flags.write_sexp "c_flags.sexp" flags.cflags;
    C.Flags.write_sexp "c_libs.sexp" flags.libs)
