[@mel.scope "document"]
external getElementById: string => Js.Nullable.t('a) = "getElementById";

external setTimeout: (unit => unit, int) => unit = "setTimeout";

let root =
  switch (getElementById("root")->Js.Nullable.toOption) {
  | Some(el) => el
  | None => failwith("Missing root element")
  };

let app = Sonner.renderToaster();

ReactDOM.Client.createRoot(root)->ReactDOM.Client.render(app);

setTimeout(() => Sonner.showError("Browser toast message"), 100);
