/* ClientSubscription.re - Handles WebSocket subscription on client side */

[@react.component]
let make =
    (
      ~children,
      ~store: Store.t,
      ~setConfig: Config.t => unit,
    ) => {
  switch%platform (Runtime.platform) {
  | Client =>
    React.useEffect1(
      () => {
        switch (store.config.premise) {
        | None => None
        | Some(premise) =>
          let premiseId = premise.id;
          let updatedAt = premise.updated_at;

          // Subscribe to WebSocket updates
          // When updates arrive, call the setter with the new config
          Js.log("Subscribing to WebSocket for premise: " ++ premiseId);

          let getConfig = () => store.config;
          Client.Socket.subscribe(setConfig, getConfig, premiseId, updatedAt->Js.Date.getTime);

          // Return cleanup function
          Some(
            () => {
              Js.log(
                "Unsubscribing from WebSocket for premise: " ++ premiseId,
                // TODO: Unsubscribe from WebSocket
              )
            },
          );
        }
      },
      [|store|],
    );

    children;
  | Server => children
  };
};
