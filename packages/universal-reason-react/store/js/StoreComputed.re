let make = (~client: Tilia.Core.deriver('a) => 'a, ~server: unit => 'a) : 'a =>
  switch%platform (Runtime.platform) {
  | Client => {
      let _ = server;
      Tilia.Core.carve(client);
    }
  | Server => {
      let _ = client;
      server();
    }
  };
