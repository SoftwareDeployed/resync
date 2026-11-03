let make = (~client: Tilia.Core.deriver('a) => 'a, ~server: unit => 'a) : 'a =>
  switch%platform () {
  | Melange => {
      let _ = server;
      Tilia.Core.carve(client);
    }
  | Native => {
      let _ = client;
      server();
    }
  };
