let get = name =>
  switch (name) {
  | "qr-code" => Some(LucideGeneratedNodes_Q.qr_code)
  | "quote" => Some(LucideGeneratedNodes_Q.quote)
  | _ => None
  };
