let handler = (config: Config.t) => {
  // Create the store record with derived fields (plain record, no Tilia on server)
  let store = Store.createPlainStore(~config, ~unit=PeriodList.Unit.defaultState);
  
  <StoreContext.Provider value=store>
    <Document config unit={PeriodList.Unit.defaultState}>
      <DebugText text={(config.premise |> Belt.Option.getUnsafe).id} />
      <App />
    </Document>
  </StoreContext.Provider>;
};
