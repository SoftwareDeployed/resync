open Ctypes
open Foreign

let version_suffix = Icu4c_config.version_suffix

let with_suffix name = name ^ version_suffix

let unique values =
  values
  |> List.fold_left (fun acc value ->
       if value = "" || List.mem value acc then acc else value :: acc) []
  |> List.rev

let split_search_path value =
  value |> String.split_on_char ':' |> List.filter (fun dir -> dir <> "")

let env_search_dirs name =
  match Sys.getenv_opt name with
  | Some value -> split_search_path value
  | None -> []

let version_major =
  if String.length version_suffix > 0 && version_suffix.[0] = '_' then
    String.sub version_suffix 1 (String.length version_suffix - 1)
  else
    version_suffix

let runtime_library_dirs =
  unique
    ((match Sys.getenv_opt "ICU_PREFIX" with
      | Some prefix -> [Filename.concat prefix "lib"]
      | None -> [])
    @ env_search_dirs "ICU_LIB_DIR"
    @ Icu4c_config.library_dirs
    @ env_search_dirs "CAML_LD_LIBRARY_PATH"
    @ env_search_dirs "DYLD_LIBRARY_PATH"
    @ env_search_dirs "DYLD_FALLBACK_LIBRARY_PATH"
    @ env_search_dirs "LD_LIBRARY_PATH"
    @ [ "/opt/homebrew/opt/icu4c/lib"
      ; "/usr/local/opt/icu4c/lib"
      ; "/usr/local/lib"
      ; "/usr/lib"
      ; "/usr/lib/aarch64-linux-gnu"
      ; "/usr/lib/x86_64-linux-gnu" ])

let library_name_candidates base_name =
  unique
    ([base_name ^ ".dylib"; base_name ^ ".so"]
    @ (if version_major = "" then [] else [base_name ^ "." ^ version_major ^ ".dylib"; base_name ^ ".so." ^ version_major])
    @ [base_name])

let library_candidates ~env_name base_name =
  let explicit =
    match Sys.getenv_opt env_name with
    | Some path -> [path]
    | None -> [] in
  let named = library_name_candidates base_name in
  unique
    (explicit
    @ List.concat_map (fun dir -> List.map (Filename.concat dir) named) runtime_library_dirs
    @ named)

let open_library ~env_name base_name =
  let candidates = library_candidates ~env_name base_name in
  let rec loop = function
    | [] ->
      failwith
        ("Unable to load ICU library: " ^ base_name ^ " (tried: "
        ^ String.concat ", " candidates ^ ")")
    | name :: rest ->
      try Dl.dlopen ~filename:name ~flags:[Dl.RTLD_NOW; Dl.RTLD_GLOBAL]
      with Dl.DL_error _ -> loop rest in
  loop candidates

let icu_uc = open_library ~env_name:"ICUUC_LIB" "libicuuc"
let icu_i18n = open_library ~env_name:"ICUI18N_LIB" "libicui18n"

(** Number formatting *)
let unumf_openForSkeletonAndLocale =
  foreign ~from:icu_i18n (with_suffix "unumf_openForSkeletonAndLocale")
    (ptr uint16_t @-> int @-> string @-> ptr int32_t @-> returning (ptr void))

let unumf_openResult =
  foreign ~from:icu_i18n (with_suffix "unumf_openResult")
    (ptr int32_t @-> returning (ptr void))

let unumf_formatDouble =
  foreign ~from:icu_i18n (with_suffix "unumf_formatDouble")
    (ptr void @-> double @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_formatInt =
  foreign ~from:icu_i18n (with_suffix "unumf_formatInt")
    (ptr void @-> int64_t @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_resultToString =
  foreign ~from:icu_i18n (with_suffix "unumf_resultToString")
    (ptr void @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let unumf_close =
  foreign ~from:icu_i18n (with_suffix "unumf_close")
    (ptr void @-> returning void)

let unumf_closeResult =
  foreign ~from:icu_i18n (with_suffix "unumf_closeResult")
    (ptr void @-> returning void)

(** DateTime formatting *)
let udat_open =
  foreign ~from:icu_i18n (with_suffix "udat_open")
    (int @-> int @-> string @-> ptr uint16_t @-> int @->
     ptr uint16_t @-> int @-> ptr int32_t @-> returning (ptr void))

let udat_close =
  foreign ~from:icu_i18n (with_suffix "udat_close")
    (ptr void @-> returning void)

let udat_format =
  foreign ~from:icu_i18n (with_suffix "udat_format")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @->
     ptr int32_t @-> returning int)

let udat_formatForFields =
  foreign ~from:icu_i18n (with_suffix "udat_formatForFields")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @->
     ptr int32_t @-> returning int)

let udatpg_open =
  foreign ~from:icu_i18n (with_suffix "udatpg_open")
    (string @-> ptr int32_t @-> returning (ptr void))

let udatpg_close =
  foreign ~from:icu_i18n (with_suffix "udatpg_close")
    (ptr void @-> returning void)

let udatpg_getBestPatternWithOptions =
  foreign ~from:icu_i18n (with_suffix "udatpg_getBestPatternWithOptions")
    (ptr void @-> ptr uint16_t @-> int @-> int @->
     ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let udatpg_getDefaultHourCycle =
  foreign ~from:icu_i18n (with_suffix "udatpg_getDefaultHourCycle")
    (ptr void @-> ptr int32_t @-> returning int)

let ufieldpositer_open =
  foreign ~from:icu_i18n (with_suffix "ufieldpositer_open")
    (ptr int32_t @-> returning (ptr void))

let ufieldpositer_next =
  foreign ~from:icu_i18n (with_suffix "ufieldpositer_next")
    (ptr void @-> ptr int @-> ptr int @-> returning int)

let ufieldpositer_close =
  foreign ~from:icu_i18n (with_suffix "ufieldpositer_close")
    (ptr void @-> returning void)

(** String conversion *)
let u_strFromUTF8 =
  foreign ~from:icu_uc (with_suffix "u_strFromUTF8")
    (ptr uint16_t @-> int @-> ptr int @-> string @-> int @->
     ptr int32_t @-> returning (ptr uint16_t))

let u_strToUTF8 =
  foreign ~from:icu_uc (with_suffix "u_strToUTF8")
    (ptr char @-> int @-> ptr int @-> ptr uint16_t @-> int @->
     ptr int32_t @-> returning (ptr char))

(** Locale handling *)
let uloc_forLanguageTag =
  foreign ~from:icu_uc (with_suffix "uloc_forLanguageTag")
    (string @-> ptr char @-> int @-> ptr int @-> ptr int32_t @-> returning int)

(** Timezone handling *)
let ucal_getCanonicalTimeZoneID =
  foreign ~from:icu_i18n (with_suffix "ucal_getCanonicalTimeZoneID")
    (ptr uint16_t @-> int @-> ptr uint16_t @-> int @->
     ptr char @-> ptr int32_t @-> returning int)

(** Error handling *)
let u_errorName =
  foreign ~from:icu_uc (with_suffix "u_errorName")
    (int32_t @-> returning string)
