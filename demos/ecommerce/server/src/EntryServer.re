let handler = (config: Config.t) => {
  let store = Store.createStore(config);
  let serializedState = Store.serializeState(config);

  <StoreContext.Provider value=store>
    <Document serializedState=serializedState> <App /> </Document>
  </StoreContext.Provider>;
};
