type attr =
  | Cx(string)
  | Cy(string)
  | D(string)
  | Fill(string)
  | Height(string)
  | Points(string)
  | R(string)
  | Rx(string)
  | Ry(string)
  | Width(string)
  | X(string)
  | X1(string)
  | X2(string)
  | Y(string)
  | Y1(string)
  | Y2(string);

type tag =
  | Circle
  | Ellipse
  | Line
  | Path
  | Polygon
  | Polyline
  | Rect;

type child = {
  key: string,
  tag: tag,
  attrs: array(attr),
};

type iconNode = array(child);
