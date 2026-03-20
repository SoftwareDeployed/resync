type style =
  | Currency
  | Decimal
  | Percent

type options = {
  style : style;
  currency : string option;
  locale : string;
  minimum_fraction_digits : int option;
  maximum_fraction_digits : int option;
  use_grouping : bool option;
}

val make_options :
  ?style:style ->
  ?currency:string ->
  ?locale:string ->
  ?minimum_fraction_digits:int ->
  ?maximum_fraction_digits:int ->
  ?use_grouping:bool ->
  unit ->
  options

val format_with_options : options -> float -> string

val format_currency :
  ?locale:string ->
  ?currency:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  float ->
  string

val format_decimal :
  ?locale:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  ?grouping:bool ->
  float ->
  string

val format_percent :
  ?locale:string ->
  ?min_fraction:int ->
  ?max_fraction:int ->
  float ->
  string
