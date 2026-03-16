let get = name =>
  switch (name) {
  | "japanese-yen" => Some(LucideGeneratedNodes_J.japanese_yen)
  | "joystick" => Some(LucideGeneratedNodes_J.joystick)
  | _ => None
  };
