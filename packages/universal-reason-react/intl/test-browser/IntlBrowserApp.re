type element;

[@mel.scope "document"]
external getElementById: string => Js.Nullable.t(element) = "getElementById";

[@mel.set]
external setTextContent: (element, string) => unit = "textContent";

let formatter =
  Intl.NumberFormatter.make(
    ~locale="en-US",
    ~style=Intl.NumberFormatter.Style.Currency,
    ~currency="USD",
    (),
  );

let formatted = formatter->Intl.NumberFormatter.format(1234.5);

let () =
  switch (getElementById("result")->Js.Nullable.toOption) {
  | Some(element) => setTextContent(element, formatted)
  | None => ()
  };
