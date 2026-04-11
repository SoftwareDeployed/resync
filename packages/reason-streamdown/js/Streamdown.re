[@platform js]
[@mel.module "streamdown"]
external streamdownComponent: React.component(Js.t({..})) = "Streamdown";

[@platform js]
let make =
    (
      ~isAnimating: option(bool)=?,
      ~mode: option(string)=?,
      ~className: option(string)=?,
      ~children: string,
      (),
    ) => {
  React.createElement(
    streamdownComponent,
    [%obj {
      isAnimating: Js.Nullable.fromOption(isAnimating),
      mode: Js.Nullable.fromOption(mode),
      className: Js.Nullable.fromOption(className),
      children: children,
    }],
  );
};

[@platform native]
let make =
    (
      ~isAnimating: option(bool)=?,
      ~mode: option(string)=?,
      ~className: option(string)=?,
      ~children: string,
      (),
    ) => {
  React.null;
};
