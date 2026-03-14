let context = React.createContext(Store.empty);

module Provider = {
  let make = React.Context.provider(context);
};

// Read store from React Context - works on both platforms
let useStore = () => React.useContext(context);
