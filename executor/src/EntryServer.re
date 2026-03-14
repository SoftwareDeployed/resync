let handler = (config: Config.t) =>
  <ConfigContext.Provider value=config>
    <Document initialConfig=config>
      <DebugText text={(config.premise |> Belt.Option.getUnsafe).id} />
      <App />
    </Document>
  </ConfigContext.Provider>;
