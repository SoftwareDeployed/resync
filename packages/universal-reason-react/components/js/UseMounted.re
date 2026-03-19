let use = () => {
  let (isMounted, setIsMounted) = React.useState(() => false);

  React.useEffect0(() => {
    setIsMounted(_previous => true);
    None;
  });

  isMounted;
};
