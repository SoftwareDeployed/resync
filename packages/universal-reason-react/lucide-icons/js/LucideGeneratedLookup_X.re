let get = name =>
  switch (name) {
  | "x-circle" => Some(LucideGeneratedNodes_C.circle_x)
  | "x-octagon" => Some(LucideGeneratedNodes_O.octagon_x)
  | "x-square" => Some(LucideGeneratedNodes_S.square_x)
  | "x" => Some(LucideGeneratedNodes_X.x)
  | _ => None
  };
