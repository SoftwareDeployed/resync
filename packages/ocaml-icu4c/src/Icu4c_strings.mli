(** UTF-8 / UTF-16 conversion utilities *)

open Ctypes

val null_uchar : Unsigned.UInt16.t ptr
(** Null pointer for UTF-16 strings *)

val null_char : char ptr
(** Null pointer for char strings *)

val buffer_overflow_error : int32
(** Buffer overflow error code *)

val check_error : int32 -> unit
(** Check if an ICU error code indicates an error and raise if so *)

val check_preflight_error : int32 -> unit
(** Check for errors, ignoring buffer overflow during preflight *)

val string_of_char_ptr : char ptr -> int -> string
(** Convert a char pointer to a string *)

val utf8_to_utf16 : string -> Unsigned.UInt16.t ptr * int
(** Convert a UTF-8 string to UTF-16. Returns the pointer and length.
    The caller is responsible for freeing the memory. *)

val utf16_to_utf8 : Unsigned.UInt16.t ptr -> int -> string
(** Convert a UTF-16 string (pointer + length) to UTF-8. *)

val utf16_slice_to_utf8 : Unsigned.UInt16.t ptr -> int -> int -> string
(** Convert a slice of a UTF-16 string to UTF-8.
    [utf16_slice_to_utf8 ptr start end] extracts characters from [start] to [end]. *)
