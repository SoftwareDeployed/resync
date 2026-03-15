type iconProps = {
  .
  "absoluteStrokeWidth": option(bool),
  "className": option(string),
  "color": option(string),
  "size": option(int),
  "strokeWidth": option(int),
};
[@platform native]
module CartIcon = {
  [@react.component]
  let make =
      (
        ~absoluteStrokeWidth: option(bool)=?,
        ~className: option(string)=?,
        ~color: option(string)=?,
        ~size: option(int)=?,
        ~strokeWidth: option(int)=?,
      ) => React.null;
};

[@platform js]
module SearchIcon = {
  [@mel.module "lucide-react"] [@react.component]
  external make:
    (
      ~absoluteStrokeWidth: bool=?,
      ~className: string=?,
      ~color: string=?,
      ~size: int=?,
      ~strokeWidth: int=?
    ) =>
    React.element =
    "SearchIcon";
};

[@platform native]
module SearchIcon = {
  [@react.component]
  let make =
      (
        ~absoluteStrokeWidth: option(bool)=?,
        ~className: option(string)=?,
        ~color: option(string)=?,
        ~size: option(int)=?,
        ~strokeWidth: option(int)=?,
      ) => React.null;
};

[@platform native]
module Calendar = {
  [@react.component]
  let make =
      (
        ~absoluteStrokeWidth: option(bool)=?,
        ~className: option(string)=?,
        ~color: option(string)=?,
        ~size: option(int)=?,
        ~strokeWidth: option(int)=?,
      ) => React.null;
};

[@platform js]
module Calendar = {
  [@mel.module "lucide-react"] [@react.component]
  external make:
    (
      ~absoluteStrokeWidth: bool=?,
      ~className: string=?,
      ~color: string=?,
      ~size: int=?,
      ~strokeWidth: int=?
    ) =>
    React.element =
    "Calendar";
};

[@platform native]
module Clock = {
  [@react.component]
  let make =
      (
        ~absoluteStrokeWidth: option(bool)=?,
        ~className: option(string)=?,
        ~color: option(string)=?,
        ~size: option(int)=?,
        ~strokeWidth: option(int)=?,
      ) => React.null;
};

[@platform js]
module Clock = {
  [@mel.module "lucide-react"] [@react.component]
  external make:
    (
      ~absoluteStrokeWidth: bool=?,
      ~className: string=?,
      ~color: string=?,
      ~size: int=?,
      ~strokeWidth: int=?
    ) =>
    React.element =
    "Clock";
};

[@platform js]
module CartIcon = {
  [@mel.module "lucide-react"] [@react.component]
  external make:
    (
      ~absoluteStrokeWidth: bool=?,
      ~className: string=?,
      ~color: string=?,
      ~size: int=?,
      ~strokeWidth: int=?
    ) =>
    React.element =
    "ShoppingCart";
};

[@platform js]
module MonitorCloud = {
  [@mel.module "lucide-react"] [@react.component]
  external make:
    (
      ~absoluteStrokeWidth: bool=?,
      ~className: string=?,
      ~color: string=?,
      ~size: int=?,
      ~strokeWidth: int=?
    ) =>
    React.element =
    "MonitorCloud";
};

[@platform native]
module MonitorCloud = {
  [@react.component]
  let make =
      (
        ~absoluteStrokeWidth: option(bool)=?,
        ~className: option(string)=?,
        ~color: option(string)=?,
        ~size: option(int)=?,
        ~strokeWidth: option(int)=?,
      ) => React.null;
};
