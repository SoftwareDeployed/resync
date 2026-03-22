open Ctypes

(** Raw ICU4C C bindings with version suffix handling *)

val version_suffix : string

val with_suffix : string -> string

(** Number formatting *)
val unumf_openForSkeletonAndLocale :
  Unsigned.UInt16.t ptr -> int -> string -> int32 ptr -> unit ptr

val unumf_openResult : int32 ptr -> unit ptr

val unumf_formatDouble :
  unit ptr -> float -> unit ptr -> int32 ptr -> unit

val unumf_formatInt :
  unit ptr -> int64 -> unit ptr -> int32 ptr -> unit

val unumf_resultToString :
  unit ptr -> Unsigned.UInt16.t ptr -> int -> int32 ptr -> int

val unumf_close : unit ptr -> unit

val unumf_closeResult : unit ptr -> unit

(** DateTime formatting *)
val udat_open :
  int -> int -> string -> Unsigned.UInt16.t ptr -> int ->
  Unsigned.UInt16.t ptr -> int -> int32 ptr -> unit ptr

val udat_close : unit ptr -> unit

val udat_format :
  unit ptr -> float -> Unsigned.UInt16.t ptr -> int -> unit ptr ->
  int32 ptr -> int

val udat_formatForFields :
  unit ptr -> float -> Unsigned.UInt16.t ptr -> int -> unit ptr ->
  int32 ptr -> int

val udatpg_open : string -> int32 ptr -> unit ptr

val udatpg_close : unit ptr -> unit

val udatpg_getBestPatternWithOptions :
  unit ptr -> Unsigned.UInt16.t ptr -> int -> int ->
  Unsigned.UInt16.t ptr -> int -> int32 ptr -> int

val udatpg_getDefaultHourCycle : unit ptr -> int32 ptr -> int

val ufieldpositer_open : int32 ptr -> unit ptr

val ufieldpositer_next : unit ptr -> int ptr -> int ptr -> int

val ufieldpositer_close : unit ptr -> unit

(** String conversion *)
val u_strFromUTF8 :
  Unsigned.UInt16.t ptr -> int -> int ptr -> string -> int ->
  int32 ptr -> Unsigned.UInt16.t ptr

val u_strToUTF8 :
  char ptr -> int -> int ptr -> Unsigned.UInt16.t ptr -> int ->
  int32 ptr -> char ptr

(** Locale handling *)
val uloc_forLanguageTag :
  string -> char ptr -> int -> int ptr -> int32 ptr -> int

(** Timezone handling *)
val ucal_getCanonicalTimeZoneID :
  Unsigned.UInt16.t ptr -> int -> Unsigned.UInt16.t ptr -> int ->
  char ptr -> int32 ptr -> int

(** Error handling *)
val u_errorName : int32 -> string
