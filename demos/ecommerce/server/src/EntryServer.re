let handler = (config: Config.t) => {
  let store = Store.createStore(config);
  let serializedState = Store.serializeState(config);

  <StoreContext.Provider value=store>
    <CartStore.Context.Provider value=CartStore.empty>
      <Document serializedState=serializedState> <App /> </Document>
    </CartStore.Context.Provider>
  </StoreContext.Provider>;
};
