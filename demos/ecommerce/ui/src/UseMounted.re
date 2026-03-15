let use = () => {
  let (isMounted, setIsMounted) = React.useState(() => false);

  React.useEffect0(() => {
    setIsMounted(_ => true);
    None;
  });

  isMounted;
};
