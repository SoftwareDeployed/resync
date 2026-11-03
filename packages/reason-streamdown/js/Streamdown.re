[@melange_only]
[@mel.module "streamdown"]
external streamdownComponent: React.component(Js.t({..})) = "Streamdown";

[@melange_only]
let make =
    (
      ~isAnimating: option(bool)=?,
      ~mode: option(string)=?,
      ~className: option(string)=?,
      ~children: string,
      (),
    ) =>
  /* The npm component can't run during native SSR (the native make returns
     React.null), so render null until the client has mounted to keep
     hydration stable; otherwise React detects a mismatch and regenerates the
     whole subtree, visibly remounting the message bubble on stream updates. */
  <ServerOrClientRender
    server={() => React.null}
    client={() =>
      React.createElement(
        streamdownComponent,
        [%obj {
          isAnimating: Js.Nullable.fromOption(isAnimating),
          mode: Js.Nullable.fromOption(mode),
          className: Js.Nullable.fromOption(className),
          children: children,
        }],
      )
    }
  />;

[@native_only]
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
