[@mel.scope "document"]
external getElementById: string => Js.Nullable.t('a) = "getElementById";

type emptyProps = Js.t({.});
[@mel.obj]
external makeEmptyProps: unit => emptyProps = "";

module ToggleQuery = {
  type params = string;
  type row = string;

  let channel = param => "hook-order:" ++ param;
  let paramsHash = param => param;
  let decodeRow = Melange_json.Primitives.string_of_json;
  let row_to_json = Melange_json.Primitives.string_to_json;
};

[@react.component]
let make = () => {
  let (params, setParams) = React.useState(() => None);

  React.useEffect0(() => {
    setParams(_ => Some("enabled"));
    None;
  });

  let _queryResult =
    Hooks.useQueryResultOption((module ToggleQuery), params, ());
  let loading =
    Hooks.useIsQueryLoadingOption((module ToggleQuery), params);
  let paramsLabel =
    switch (params) {
    | None => "disabled"
    | Some(_) => "enabled"
    };
  let text = paramsLabel ++ ":" ++ (loading ? "loading" : "not-loading");

  <div id="hook-order-result"> {React.string(text)} </div>;
};

let root =
  switch (getElementById("root")->Js.Nullable.toOption) {
  | Some(el) => el
  | None => failwith("Missing root element")
  };

ReactDOM.Client.createRoot(root)
->ReactDOM.Client.render(React.createElement(make, makeEmptyProps()));
