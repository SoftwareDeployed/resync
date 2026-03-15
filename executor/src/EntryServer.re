let handler = (config: Config.t) => {
  let store = Store.createStore(~config, ~unit=PeriodList.Unit.defaultState);
  
  <StoreContext.Provider value=store>
    <Document config unit={PeriodList.Unit.defaultState}>
      <DebugText text={(config.premise |> Belt.Option.getUnsafe).id} />
      <App />
    </Document>
  </StoreContext.Provider>;
};
