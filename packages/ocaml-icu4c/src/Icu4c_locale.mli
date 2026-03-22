(** Locale and timezone normalization utilities *)

val normalize_locale : string -> string
(** Normalize a locale string using ICU's locale handling.
    Throws [Invalid_argument] if the locale is invalid or empty. *)

val normalize_time_zone : string -> string
(** Normalize a timezone string using ICU's timezone handling.
    Throws [Invalid_argument] if the timezone is invalid or empty. *)
