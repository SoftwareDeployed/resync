open Ctypes
open Foreign

let version_suffix = Icu4c_config.version_suffix

let with_suffix name = name ^ version_suffix

(** Number formatting *)
let unumf_openForSkeletonAndLocale =
  foreign (with_suffix "unumf_openForSkeletonAndLocale")
    (ptr uint16_t @-> int @-> string @-> ptr int32_t @-> returning (ptr void))

let unumf_openResult =
  foreign (with_suffix "unumf_openResult")
    (ptr int32_t @-> returning (ptr void))

let unumf_formatDouble =
  foreign (with_suffix "unumf_formatDouble")
    (ptr void @-> double @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_formatInt =
  foreign (with_suffix "unumf_formatInt")
    (ptr void @-> int64_t @-> ptr void @-> ptr int32_t @-> returning void)

let unumf_resultToString =
  foreign (with_suffix "unumf_resultToString")
    (ptr void @-> ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let unumf_close =
  foreign (with_suffix "unumf_close")
    (ptr void @-> returning void)

let unumf_closeResult =
  foreign (with_suffix "unumf_closeResult")
    (ptr void @-> returning void)

(** DateTime formatting *)
let udat_open =
  foreign (with_suffix "udat_open")
    (int @-> int @-> string @-> ptr uint16_t @-> int @->
     ptr uint16_t @-> int @-> ptr int32_t @-> returning (ptr void))

let udat_close =
  foreign (with_suffix "udat_close")
    (ptr void @-> returning void)

let udat_format =
  foreign (with_suffix "udat_format")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @->
     ptr int32_t @-> returning int)

let udat_formatForFields =
  foreign (with_suffix "udat_formatForFields")
    (ptr void @-> double @-> ptr uint16_t @-> int @-> ptr void @->
     ptr int32_t @-> returning int)

let udatpg_open =
  foreign (with_suffix "udatpg_open")
    (string @-> ptr int32_t @-> returning (ptr void))

let udatpg_close =
  foreign (with_suffix "udatpg_close")
    (ptr void @-> returning void)

let udatpg_getBestPatternWithOptions =
  foreign (with_suffix "udatpg_getBestPatternWithOptions")
    (ptr void @-> ptr uint16_t @-> int @-> int @->
     ptr uint16_t @-> int @-> ptr int32_t @-> returning int)

let udatpg_getDefaultHourCycle =
  foreign (with_suffix "udatpg_getDefaultHourCycle")
    (ptr void @-> ptr int32_t @-> returning int)

let ufieldpositer_open =
  foreign (with_suffix "ufieldpositer_open")
    (ptr int32_t @-> returning (ptr void))

let ufieldpositer_next =
  foreign (with_suffix "ufieldpositer_next")
    (ptr void @-> ptr int @-> ptr int @-> returning int)

let ufieldpositer_close =
  foreign (with_suffix "ufieldpositer_close")
    (ptr void @-> returning void)

(** String conversion *)
let u_strFromUTF8 =
  foreign (with_suffix "u_strFromUTF8")
    (ptr uint16_t @-> int @-> ptr int @-> string @-> int @->
     ptr int32_t @-> returning (ptr uint16_t))

let u_strToUTF8 =
  foreign (with_suffix "u_strToUTF8")
    (ptr char @-> int @-> ptr int @-> ptr uint16_t @-> int @->
     ptr int32_t @-> returning (ptr char))

(** Locale handling *)
let uloc_forLanguageTag =
  foreign (with_suffix "uloc_forLanguageTag")
    (string @-> ptr char @-> int @-> ptr int @-> ptr int32_t @-> returning int)

(** Timezone handling *)
let ucal_getCanonicalTimeZoneID =
  foreign (with_suffix "ucal_getCanonicalTimeZoneID")
    (ptr uint16_t @-> int @-> ptr uint16_t @-> int @->
     ptr char @-> ptr int32_t @-> returning int)

(** Error handling *)
let u_errorName =
  foreign (with_suffix "u_errorName")
    (int32_t @-> returning string)
