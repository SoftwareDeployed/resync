exception MissingContext(string);

module Params = {
  type entry = (string, string);
  type t = list(entry);

  let empty: t = [];

  let ofList = entries => entries;

  let toList = params => params;

  let add = (params, key, value) => {
    let remaining =
      params |> List.filter(((existingKey, _value)) => existingKey != key);
    [(key, value), ...remaining];
  };

  let merge = (left, right) =>
    right |> List.fold_left((acc, (key, value)) => add(acc, key, value), left);

  let find = (key, params) =>
    params
    |> List.find_map(((existingKey, value)) =>
         existingKey == key ? Some(value) : None
       );

  let get = find;

  let has = (key, params) => params |> List.exists(((existingKey, _value)) => existingKey == key);
};

module SearchParams = {
  type entry = (string, string);
  type t = list(entry);

  let empty: t = [];

  let ofList = entries => entries;

  let toList = searchParams => searchParams;

  let isAlphaNumeric = char =>
    (char >= 'a' && char <= 'z') || (char >= 'A' && char <= 'Z') || (char >= '0' && char <= '9');

  let isUnreserved = char =>
    isAlphaNumeric(char) || char == '-' || char == '_' || char == '.' || char == '~';

  let hexDigit = value =>
    value < 10 ? Char.chr(Char.code('0') + value) : Char.chr(Char.code('A') + value - 10);

  let hexValue = char =>
    if (char >= '0' && char <= '9') {
      Some(Char.code(char) - Char.code('0'));
    } else if (char >= 'A' && char <= 'F') {
      Some(Char.code(char) - Char.code('A') + 10);
    } else if (char >= 'a' && char <= 'f') {
      Some(Char.code(char) - Char.code('a') + 10);
    } else {
      None;
    };

  let encodeComponent = value => {
    let buffer = Buffer.create(String.length(value) * 3);
    value
    |> String.iter(char =>
         if (isUnreserved(char)) {
           Buffer.add_char(buffer, char);
         } else if (char == ' ') {
           Buffer.add_char(buffer, '+');
         } else {
           let code = Char.code(char);
           Buffer.add_char(buffer, '%');
           Buffer.add_char(buffer, hexDigit(code / 16));
           Buffer.add_char(buffer, hexDigit(code mod 16));
         }
       );
    Buffer.contents(buffer);
  };

  let decodeComponent = value => {
    let buffer = Buffer.create(String.length(value));
    let valueLength = String.length(value);

    let rec loop = index =>
      if (index >= valueLength) {
        ();
      } else {
        switch (value.[index]) {
        | '+' => {
            Buffer.add_char(buffer, ' ');
            loop(index + 1);
          }
        | '%' =>
          if (index + 2 < valueLength) {
            switch (hexValue(value.[index + 1]), hexValue(value.[index + 2])) {
            | (Some(high), Some(low)) => {
                Buffer.add_char(buffer, Char.chr(high * 16 + low));
                loop(index + 3);
              }
            | _ => {
                Buffer.add_char(buffer, '%');
                loop(index + 1);
              }
            };
          } else {
            Buffer.add_char(buffer, '%');
            loop(index + 1);
          }
        | char => {
            Buffer.add_char(buffer, char);
            loop(index + 1);
          }
        };
      };

    loop(0);
    Buffer.contents(buffer);
  };

  let searchWithoutPrefix = search =>
    String.starts_with(search, ~prefix="?")
      ? search->String.sub(1, String.length(search) - 1) : search;

  let splitEntry = entry => {
    let entryLength = String.length(entry);

    let rec findSeparator = index =>
      index >= entryLength
        ? None
        : entry.[index] == '=' ? Some(index) : findSeparator(index + 1);

    switch (findSeparator(0)) {
    | None => (entry, "")
    | Some(index) =>
      (
        entry->String.sub(0, index),
        entry->String.sub(index + 1, entryLength - index - 1),
      )
    };
  };

  let parse = search => {
    let rawSearch = searchWithoutPrefix(search);

    rawSearch == ""
      ? []
      : rawSearch
        |> String.split_on_char('&')
        |> List.filter_map(entry =>
             entry == ""
               ? None
               : {
                   let (key, value) = splitEntry(entry);
                   Some((decodeComponent(key), decodeComponent(value)));
                 }
            );
  };

  let get = (key, searchParams) =>
    searchParams
    |> List.find_map(((existingKey, value)) =>
         existingKey == key ? Some(value) : None
       );

  let find = get;

  let getAll = (key, searchParams) =>
    searchParams
    |> List.filter_map(((existingKey, value)) => existingKey == key ? Some(value) : None);

  let has = (key, searchParams) =>
    searchParams |> List.exists(((existingKey, _value)) => existingKey == key);

  let delete = (searchParams, key) =>
    searchParams |> List.filter(((existingKey, _value)) => existingKey != key);

  let append = (searchParams, key, value) => List.append(searchParams, [(key, value)]);

  let set = (searchParams, key, value) => append(delete(searchParams, key), key, value);

  let toString = searchParams =>
    searchParams
    |> List.map(((key, value)) => {
         let encodedKey = encodeComponent(key);
         let encodedValue = encodeComponent(value);
         value == "" ? encodedKey : encodedKey ++ "=" ++ encodedValue;
       })
    |> String.concat("&");

  let toSearch = searchParams => {
    let serialized = toString(searchParams);
    serialized == "" ? "" : "?" ++ serialized;
  };
};

type headTag =
  | MetaTag(string, string)
  | PropertyTag(string, string)
  | LinkTag(string, string);

type titleResolver = (
  ~pathname: string,
  ~params: Params.t,
  ~searchParams: SearchParams.t,
) => string;

type titleResolverWithState('state) =
  (
    ~pathname: string,
    ~params: Params.t,
    ~searchParams: SearchParams.t,
    ~state: 'state,
  ) =>
  string;

type headTagsResolver =
  (~pathname: string, ~params: Params.t, ~searchParams: SearchParams.t) => list(headTag);

type headTagsResolverWithState('state) =
  (
    ~pathname: string,
    ~params: Params.t,
    ~searchParams: SearchParams.t,
    ~state: 'state,
  ) =>
  list(headTag);

type hydrationState = {
  basePath: string,
  pathname: string,
  search: string,
};

type documentConfig = {
  title: string,
  lang: string,
  stylesheets: array(string),
  scripts: array(string),
  head: option(React.element),
  headTags: list(headTag),
  beforeMain: option(React.element),
  afterMain: option(React.element),
  rootId: string,
};

module type LAYOUT = {
  let make:
    (
      ~children: React.element,
      ~params: Params.t,
      ~searchParams: SearchParams.t,
      unit,
    ) =>
    React.element;
};

module type PAGE = {
  let make: (~params: Params.t, ~searchParams: SearchParams.t, unit) => React.element;
};

module type NOT_FOUND = {
  let make: (~pathname: string, unit) => React.element;
};

type routeConfig('state) = {
  id: option(string),
  path: string,
  layout: option(module LAYOUT),
  page: option(module PAGE),
  title: option(string),
  resolveTitle: option(titleResolver),
  resolveTitleWithState: option(titleResolverWithState('state)),
  headTags: list(headTag),
  resolveHeadTags: option(headTagsResolver),
  resolveHeadTagsWithState: option(headTagsResolverWithState('state)),
  children: list(routeConfig('state)),
};

type t('state) = {
  document: documentConfig,
  notFound: option(module NOT_FOUND),
  routes: list(routeConfig('state)),
};

type matchResult('state) = {
  pathname: string,
  params: Params.t,
  searchParams: SearchParams.t,
  routes: list(routeConfig('state)),
};

type routerApi = {
  push: string => unit,
  replace: string => unit,
  pushRoute:
    (~id: string, ~params: Params.t=?, ~searchParams: SearchParams.t=?, unit) => unit,
  replaceRoute:
    (~id: string, ~params: Params.t=?, ~searchParams: SearchParams.t=?, unit) => unit,
};

type contextValueWithState('state) = {
  router: t('state),
  basePath: string,
  pathname: string,
  search: string,
  searchParams: SearchParams.t,
  params: Params.t,
  matchResult: option(matchResult('state)),
};

type contextValue = {
  router: t(unit),
  basePath: string,
  pathname: string,
  search: string,
  searchParams: SearchParams.t,
  params: Params.t,
  matchResult: option(matchResult(unit)),
};

let context: React.Context.t(option(contextValue)) = React.createContext(None);
let provider = React.Context.provider(context);

let castToUnitRouter = (router: t('state)) => (Obj.magic(router): t(unit));

let castToUnitMatchResult = (matchResult: option(matchResult('state))) =>
  switch (matchResult) {
  | None => None
  | Some(result) => Some(Obj.magic(result): matchResult(unit))
  };

let hydrationScriptId = "universal-router-state";

[@platform js] [@mel.scope "document"]
external getElementById: string => Js.Nullable.t('a) = "getElementById";

[@platform js] [@mel.get]
external textContent: 'a => Js.Nullable.t(string) = "textContent";

[@platform js]
external documentRef: 'a = "document";

[@platform js] [@mel.set]
external setDocumentTitleValue: ('a, string) => unit = "title";

[@platform js] [@mel.get]
external documentHead: 'a => 'b = "head";

[@platform js] [@mel.send]
external querySelectorAll: ('a, string) => 'b = "querySelectorAll";

[@platform js] [@mel.send]
external forEachNode: ('a, 'b => unit) => unit = "forEach";

[@platform js] [@mel.send]
external removeNode: 'a => unit = "remove";

[@platform js] [@mel.scope "document"]
external createElement: string => 'a = "createElement";

[@platform js] [@mel.send]
external setAttribute: ('a, string, string) => unit = "setAttribute";

[@platform js] [@mel.send]
external appendChild: ('a, 'b) => unit = "appendChild";

let filterEmptySegments = segments =>
  segments |> List.filter(segment => segment != "");

let pathOfSegments = segments =>
  switch (segments) {
  | [] => "/"
  | _ => "/" ++ String.concat("/", segments)
  };

let segmentsOfPath = path => path |> String.split_on_char('/') |> filterEmptySegments;

let normalizePath = path => path |> segmentsOfPath |> pathOfSegments;

let normalizeBasePath = basePath => {
  let trimmed = basePath |> String.trim;
  trimmed == "" ? "/" : normalizePath(trimmed);
};

let normalizeRoutePath = path => {
  let trimmed = path |> String.trim;
  (trimmed == "" || trimmed == "/") ? "" : trimmed |> normalizePath;
};

let routeSegments = routePath =>
  routePath |> normalizeRoutePath |> segmentsOfPath;

let document = (
  ~title="Create Reason React Tailwind",
  ~lang="en",
  ~stylesheets=[|"/style.css"|],
  ~scripts=[|"/app.js"|],
  ~head: option(React.element)=?,
  ~headTags=[],
  ~beforeMain: option(React.element)=?,
  ~afterMain: option(React.element)=?,
  ~rootId="root",
  (),
) => {
  title,
  lang,
  stylesheets,
  scripts,
  head,
  headTags,
  beforeMain,
  afterMain,
  rootId,
};

let metaTag = (~name, ~content, ()) => MetaTag(name, content);

let propertyTag = (~property, ~content, ()) => PropertyTag(property, content);

let linkTag = (~rel, ~href, ()) => LinkTag(rel, href);

let route = (
  ~id: option(string)=?,
  ~path,
  ~layout=?,
  ~page=?,
  ~title: option(string)=?,
  ~resolveTitle: option(titleResolver)=?,
  ~resolveTitleWithState: option(titleResolverWithState('state))=?,
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  ~resolveHeadTagsWithState: option(headTagsResolverWithState('state))=?,
  children,
  (),
) => {
  id,
  path: normalizeRoutePath(path),
  layout,
  page,
  title,
  resolveTitle,
  resolveTitleWithState,
  headTags,
  resolveHeadTags,
  resolveHeadTagsWithState,
  children,
};

let group = (
  ~id: option(string)=?,
  ~path="",
  ~layout=?,
  ~title: option(string)=?,
  ~resolveTitle: option(titleResolver)=?,
  ~resolveTitleWithState: option(titleResolverWithState('state))=?,
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  ~resolveHeadTagsWithState: option(headTagsResolverWithState('state))=?,
  children,
  (),
) =>
  route(
    ~id?,
    ~path,
    ~layout?,
    ~title?,
    ~resolveTitle?,
    ~resolveTitleWithState?,
    ~headTags,
    ~resolveHeadTags?,
    ~resolveHeadTagsWithState?,
    children,
    (),
  );

let index = (
  ~id: option(string)=?,
  ~layout=?,
  ~page,
  ~title: option(string)=?,
  ~resolveTitle: option(titleResolver)=?,
  ~resolveTitleWithState: option(titleResolverWithState('state))=?,
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  ~resolveHeadTagsWithState: option(headTagsResolverWithState('state))=?,
  (),
) =>
  route(
    ~id?,
    ~path="",
    ~layout?,
    ~page,
    ~title?,
    ~resolveTitle?,
    ~resolveTitleWithState?,
    ~headTags,
    ~resolveHeadTags?,
    ~resolveHeadTagsWithState?,
    [],
    (),
  );

let create = (~document: documentConfig=document(), ~notFound=?, routes) => {
  document,
  notFound,
  routes,
};

let serializeHydrationState = (state: hydrationState) =>
  state.basePath ++ "\n" ++ state.pathname ++ "\n" ++ state.search;

let parseHydrationState = value => {
  switch (value |> String.split_on_char('\n')) {
  | [basePath, pathname, search, ..._] =>
    Some({basePath: normalizeBasePath(basePath), pathname: normalizePath(pathname), search})
  | [basePath, pathname] =>
    Some({basePath: normalizeBasePath(basePath), pathname: normalizePath(pathname), search: ""})
  | _ => None
  };
};

let readHydrationState = () =>
  switch%platform (Runtime.platform) {
  | Client =>
    switch (getElementById(hydrationScriptId)->Js.Nullable.toOption) {
    | None => None
    | Some(node) =>
      switch (textContent(node)->Js.Nullable.toOption) {
      | None => None
      | Some(value) => parseHydrationState(value)
      }
    }
  | Server => None
  };

let stripBasePath = (~basePath, ~path) => {
  let normalizedBasePath = normalizeBasePath(basePath);
  let normalizedPath = normalizePath(path);

  if (normalizedBasePath == "/") {
    Some(normalizedPath);
  } else if (normalizedPath == normalizedBasePath) {
    Some("/");
  } else {
    let prefix = normalizedBasePath ++ "/";
    String.starts_with(normalizedPath, ~prefix)
      ? Some(
          normalizedPath->String.sub(
            String.length(normalizedBasePath),
            String.length(normalizedPath) - String.length(normalizedBasePath),
          ),
        )
      : None;
  };
};

let prefixBasePath = (~basePath, ~path) => {
  let normalizedBasePath = normalizeBasePath(basePath);
  let normalizedPath = normalizePath(path);

  if (normalizedBasePath == "/") {
    normalizedPath;
  } else if (normalizedPath == "/") {
    normalizedBasePath;
  } else {
    normalizedBasePath ++ normalizedPath;
  };
};

let makeServerUrl = (~basePath, ~pathname, ~search="", ()) => {
  ReasonReactRouter.path: prefixBasePath(~basePath, ~path=pathname) |> segmentsOfPath,
  hash: "",
  search,
};

let splitHref = href => {
  let hrefLength = String.length(href);

  let rec findSuffixIndex = index =>
    index >= hrefLength
      ? None
      : switch (href.[index]) {
        | '?' | '#' => Some(index)
        | _ => findSuffixIndex(index + 1)
        };

  switch (findSuffixIndex(0)) {
  | None => (href, "")
  | Some(index) =>
    (
      href->String.sub(0, index),
      href->String.sub(index, hrefLength - index),
    )
  };
};

let normalizeHrefPath = path => {
  let trimmed = path |> String.trim;
  trimmed == "" ? "/" : String.starts_with(trimmed, ~prefix="/") ? normalizePath(trimmed) : normalizePath("/" ++ trimmed);
};

let isExternalHref = href =>
  String.starts_with(href, ~prefix="http://") || String.starts_with(href, ~prefix="https://") || String.starts_with(href, ~prefix="//") || String.starts_with(href, ~prefix="mailto:") || String.starts_with(href, ~prefix="tel:") || String.starts_with(href, ~prefix="#");

let resolveHref = (~basePath, href) => {
  if (href == "" || isExternalHref(href)) {
    href;
  } else {
    let (path, suffix) = splitHref(href);
    let normalizedPath = normalizeHrefPath(path);
    let resolvedPath =
      switch (stripBasePath(~basePath, ~path=normalizedPath)) {
      | Some(_) => normalizedPath
      | None => prefixBasePath(~basePath, ~path=normalizedPath)
      };
    resolvedPath ++ suffix;
  };
};

let pathnameOfHref = href => {
  let (path, _suffix) = splitHref(href);
  normalizeHrefPath(path);
};

let candidateBasePaths = path => {
  let segments = path |> normalizePath |> segmentsOfPath;
  let prefixes =
    segments
    |> List.fold_left(
         (acc, segment) => {
            let previous = switch (acc) { | [head, ..._] => head | [] => [] };
            [List.append(previous, [segment]), ...acc];
          },
          [[]],
        );

  prefixes
  |> List.map(pathOfSegments)
  |> List.sort_uniq(String.compare)
  |> List.sort((left, right) => String.length(right) - String.length(left));
};

let headTagKey = headTag =>
  switch (headTag) {
  | MetaTag(name, _content) => "meta:name:" ++ name
  | PropertyTag(property, _content) => "meta:property:" ++ property
  | LinkTag(rel, _href) => "link:" ++ rel
  };

let serializeHeadTag = headTag =>
  switch (headTag) {
  | MetaTag(name, content) => "meta:name:" ++ name ++ ":" ++ content
  | PropertyTag(property, content) =>
    "meta:property:" ++ property ++ ":" ++ content
  | LinkTag(rel, href) => "link:" ++ rel ++ ":" ++ href
  };

let mergeHeadTags = (existingHeadTags, nextHeadTags) =>
  nextHeadTags
  |> List.fold_left((acc, headTag) => {
       let key = headTagKey(headTag);
       let remaining =
         acc |> List.filter(existingHeadTag => headTagKey(existingHeadTag) != key);
       List.append(remaining, [headTag]);
     }, existingHeadTags);

let matchPattern = (patternSegments, pathSegments) => {
  let rec loop = (patternSegments, pathSegments, params) =>
    switch (patternSegments, pathSegments) {
    | ([], remainingSegments) => Some((remainingSegments, List.rev(params)))
    | ([patternSegment, ...remainingPattern], [pathSegment, ...remainingPath]) =>
      if (String.starts_with(patternSegment, ~prefix=":")) {
        let key =
          patternSegment->String.sub(1, String.length(patternSegment) - 1);
        loop(remainingPattern, remainingPath, [(key, pathSegment), ...params]);
      } else if (patternSegment == pathSegment) {
        loop(remainingPattern, remainingPath, params);
      } else {
        None;
      }
    | _ => None
    };

  loop(patternSegments, pathSegments, []);
};

let matchPath = (~router: t('state), ~pathname, ~searchParams=SearchParams.empty, ()) => {
  let pathSegments = pathname |> normalizePath |> segmentsOfPath;

  let rec matchRoutes =
          (
            routes: list(routeConfig('state)),
            remainingSegments,
            params,
            matchedRoutes,
          ) =>
    switch (routes) {
    | [] => None
    | [currentRoute, ...remainingRoutes] =>
      switch (matchRoute(currentRoute, remainingSegments, params, matchedRoutes)) {
      | Some(result) => Some(result)
      | None => matchRoutes(remainingRoutes, remainingSegments, params, matchedRoutes)
      }
    }
  and matchRoute =
        (
           currentRoute: routeConfig('state),
           remainingSegments,
           params,
           matchedRoutes,
        ) => {
    switch (matchPattern(routeSegments(currentRoute.path), remainingSegments)) {
    | None => None
    | Some((nextSegments, matchedParams)) =>
      let nextParams = Params.merge(params, matchedParams);
      let nextMatchedRoutes = [currentRoute, ...matchedRoutes];

      let childMatch = () =>
        matchRoutes(currentRoute.children, nextSegments, nextParams, nextMatchedRoutes);

      if (nextSegments == []) {
        switch (childMatch()) {
          | Some(result) => Some(result)
          | None =>
              Some({
              pathname: normalizePath(pathname),
              params: nextParams,
              searchParams,
              
              routes: List.rev(nextMatchedRoutes),
            })
        };
      } else {
        childMatch();
      }
    }
  };

  matchRoutes(router.routes, pathSegments, Params.empty, []);
};

let matchMountedPath = (
  ~router: t('state),
  ~basePath,
  ~pathname,
  ~searchParams=SearchParams.empty,
  (),
) =>
  switch (stripBasePath(~basePath, ~path=pathname)) {
  | None => None
  | Some(rootlessPath) => matchPath(~router, ~pathname=rootlessPath, ~searchParams, ())
  };

let rec findPatternForId = (routes: list(routeConfig('state)), parentSegments, id) =>
  switch (routes) {
  | [] => None
  | [(currentRoute: routeConfig('state)), ...remainingRoutes] =>
    let nextSegments = List.append(parentSegments, routeSegments(currentRoute.path));
    if (currentRoute.id == Some(id)) {
      Some(nextSegments);
    } else {
      switch (findPatternForId(currentRoute.children, nextSegments, id)) {
      | Some(pathSegments) => Some(pathSegments)
      | None => findPatternForId(remainingRoutes, parentSegments, id)
      };
    }
  };

let buildPath = (~patternSegments, ~params=Params.empty, ~searchParams=SearchParams.empty, ()) => {
  let segments =
    patternSegments
    |> List.map(segment =>
         if (String.starts_with(segment, ~prefix=":")) {
           let key = segment->String.sub(1, String.length(segment) - 1);
           Params.find(key, params) |> Option.value(~default="")
         } else {
           segment
         }
        );

  let path = pathOfSegments(segments);
  let queryString = SearchParams.toString(searchParams);
  queryString == "" ? path : path ++ "?" ++ queryString;
};

let routeHref = (
  ~router: t('state),
  ~basePath="/",
  ~id,
  ~params=Params.empty,
  ~searchParams=SearchParams.empty,
  (),
) =>
  switch (findPatternForId(router.routes, [], id)) {
  | None => None
  | Some(patternSegments) =>
    Some(
      prefixBasePath(
        ~basePath,
        ~path=buildPath(~patternSegments, ~params, ~searchParams, ()),
      ),
    )
  };

let renderMatched = (~matchResult: matchResult('state), ~router: t('state)) => {
  let _ = router;
  let params = matchResult.params;
  let searchParams = matchResult.searchParams;

  let rec loop = routes =>
    switch (routes) {
    | [] => React.null
    | [currentRoute] => {
        let leaf =
          switch (currentRoute.page) {
          | None => React.null
          | Some(page) =>
            module Page = (val page: PAGE);
            Page.make(~params, ~searchParams, ())
          };

        switch (currentRoute.layout) {
        | None => leaf
        | Some(layout) =>
          module Layout = (val layout: LAYOUT);
          Layout.make(~children=leaf, ~params, ~searchParams, ())
        }
      }
    | [currentRoute, ...remainingRoutes] => {
        let children = loop(remainingRoutes);
        switch (currentRoute.layout) {
      | None => children
      | Some(layout) =>
        module Layout = (val layout: LAYOUT);
        Layout.make(~children, ~params, ~searchParams, ())
      }
    }
    };

  loop(matchResult.routes);
};

let renderNotFound = (~router: t('state), ~pathname) =>
  switch (router.notFound) {
  | None => React.null
  | Some(notFound) =>
    module NotFound = (val notFound: NOT_FOUND);
    NotFound.make(~pathname, ())
  };

let resolveTitle =
    (
      ~router: t('state),
      ~matchResult: option(matchResult('state)),
      ~state: option('state),
    ) =>
  switch (matchResult) {
  | None => router.document.title
  | Some(result) =>
    result.routes
    |> List.fold_left(
          (resolvedTitle, currentRoute) =>
            switch (currentRoute.resolveTitleWithState) {
            | Some(resolveTitleWithState) =>
              switch (state) {
              | Some(stateValue) =>
                resolveTitleWithState(
                  ~pathname=result.pathname,
                  ~params=result.params,
                  ~searchParams=result.searchParams,
                  ~state=stateValue,
                )
              | None =>
                switch (currentRoute.resolveTitle) {
                | Some(resolveTitle) =>
                  resolveTitle(
                    ~pathname=result.pathname,
                    ~params=result.params,
                    ~searchParams=result.searchParams,
                  )
                | None =>
                  switch (currentRoute.title) {
                  | Some(title) => title
                  | None => resolvedTitle
                  }
                }
              }
            | None =>
              switch (currentRoute.resolveTitle) {
              | Some(resolveTitle) =>
                resolveTitle(
                  ~pathname=result.pathname,
                  ~params=result.params,
                  ~searchParams=result.searchParams,
                )
              | None =>
                switch (currentRoute.title) {
                | Some(title) => title
                | None => resolvedTitle
                }
              }
            },
          router.document.title,
        )
  };

let resolveHeadTags =
    (
      ~router: t('state),
      ~matchResult: option(matchResult('state)),
      ~state: option('state),
    ) =>
  switch (matchResult) {
  | None => router.document.headTags
  | Some(result) =>
    result.routes
    |> List.fold_left(
         (resolvedHeadTags, currentRoute) => {
            let currentRouteHeadTags =
              switch (currentRoute.resolveHeadTagsWithState, state) {
              | (Some(resolveHeadTagsWithState), Some(stateValue)) =>
                mergeHeadTags(
                  currentRoute.headTags,
                  resolveHeadTagsWithState(
                    ~pathname=result.pathname,
                    ~params=result.params,
                    ~searchParams=result.searchParams,
                    ~state=stateValue,
                  ),
                )
              | (_, _) =>
                switch (currentRoute.resolveHeadTags) {
                | Some(resolveHeadTags) =>
                  mergeHeadTags(
                    currentRoute.headTags,
                    resolveHeadTags(
                      ~pathname=result.pathname,
                      ~params=result.params,
                      ~searchParams=result.searchParams,
                    ),
                  )
                | None => currentRoute.headTags
                }
              };
            mergeHeadTags(resolvedHeadTags, currentRouteHeadTags);
         },
           router.document.headTags,
         )
  };

let headTagId = index => "universal-router-head-" ++ string_of_int(index);

let renderHeadTagElement = (~index, headTag) => {
  let id = headTagId(index);
  switch (headTag) {
  | MetaTag(name, content) => <meta key=id id name content />
  | PropertyTag(property, content) => <meta key=id id property content />
  | LinkTag(rel, href) => <link key=id id rel href />
  };
};

let renderHeadTags = headTags =>
  switch (headTags) {
  | [] => React.null
  | _ =>
    headTags
    |> Array.of_list
    |> Array.mapi((index, headTag) => renderHeadTagElement(~index, headTag))
    |> React.array
  };

let mergeHeadContent = (baseHead, headTags) => {
  let renderedHeadTags = renderHeadTags(headTags);
  switch (baseHead, headTags) {
  | (None, []) => None
  | (Some(head), []) => Some(head)
  | (None, _) => Some(renderedHeadTags)
  | (Some(head), _) =>
    Some(
      <React.Fragment>
        head
        renderedHeadTags
      </React.Fragment>,
    )
  };
};

let applyHeadTags = headTags =>
  switch%platform (Runtime.platform) {
  | Client => {
      let head = documentHead(documentRef);
      head
      ->querySelectorAll("[id^='universal-router-head-']")
      ->forEachNode(node => node->removeNode);

      headTags
      |> List.iteri((index, headTag) => {
           let element =
             switch (headTag) {
             | MetaTag(_, _) | PropertyTag(_, _) => createElement("meta")
             | LinkTag(_, _) => createElement("link")
             };

           setAttribute(element, "id", headTagId(index));

           switch (headTag) {
           | MetaTag(name, content) => {
               setAttribute(element, "name", name);
               setAttribute(element, "content", content);
             }
           | PropertyTag(property, content) => {
               setAttribute(element, "property", property);
               setAttribute(element, "content", content);
             }
           | LinkTag(rel, href) => {
               setAttribute(element, "rel", rel);
               setAttribute(element, "href", href);
             }
           };

           head->appendChild(element);
         });

      ();
    }
  | Server => ()
  };

let mergeAfterMain = (afterMain, appendedElement) =>
  switch (afterMain) {
  | None => Some(appendedElement)
  | Some(existingElement) =>
    Some(
      <React.Fragment>
        existingElement
        appendedElement
      </React.Fragment>,
    )
  };

let renderHydrationScript = state =>
  <script
    id=hydrationScriptId
    type_="text/plain"
    dangerouslySetInnerHTML={"__html": serializeHydrationState(state)}
  />;

let renderDocument = (
   ~router: t('state),
   ~children,
   ~basePath="/",
   ~pathname="/",
   ~search="",
   ~serializedState="",
   ~serializedQueries="",
   ~state: option('state)=?,
   (),
) => {
  let searchParams = SearchParams.parse(search);
  let matchResult = matchPath(~router, ~pathname, ~searchParams, ());
  let title = resolveTitle(~router, ~matchResult, ~state);
  let head =
    mergeHeadContent(router.document.head, resolveHeadTags(~router, ~matchResult, ~state));
  let hydrationState = {basePath: normalizeBasePath(basePath), pathname, search};
  let afterMain =
    mergeAfterMain(router.document.afterMain, renderHydrationScript(hydrationState));

  <Document
    title
    lang=router.document.lang
    stylesheets=router.document.stylesheets
    scripts=router.document.scripts
    serializedState
    serializedQueries
    rootId=router.document.rootId
    head=?head
    beforeMain=?router.document.beforeMain
    afterMain=?afterMain>
    children
  </Document>;
};

let useRouterState = () =>
  switch (React.useContext(context)) {
  | Some(value) => value
  | None => raise(MissingContext("UniversalRouter requires the router provider"))
  };

let joinClassNames = classNames =>
  classNames
  |> List.filter(className => className != "")
  |> String.concat(" ");

let useParams = () => useRouterState().params;
let useSearchParams = () => useRouterState().searchParams;
let useSearch = () => useRouterState().search;
let useMatch = () => useRouterState().matchResult;
let usePathname = () => useRouterState().pathname;
let useBasePath = () => useRouterState().basePath;

let useIsActiveRoute = (~id, ~exact=false, ()) => {
  let routerState = useRouterState();

  switch (routerState.matchResult) {
  | None => false
  | Some(result) =>
    exact
      ? switch (List.rev(result.routes)) {
        | [route, ..._] => route.id == Some(id)
        | [] => false
        }
       : result.routes |> List.exists(route => route.id == Some(id))
  };
};

let useRouteHref = (~id, ~params=Params.empty, ~searchParams=SearchParams.empty, ()) => {
  let routerState = useRouterState();
  routeHref(
    ~router=routerState.router,
    ~basePath=routerState.basePath,
    ~id,
    ~params,
    ~searchParams,
    (),
  );
};

let navigateHref = (~replace=false, href) =>
  replace ? ReasonReactRouter.replace(href) : ReasonReactRouter.push(href);

let useNavigateToRoute = () => {
  let routerState = useRouterState();
  (~replace=false, ~id, ~params=Params.empty, ~searchParams=SearchParams.empty, ()) =>
    switch (
      routeHref(
        ~router=routerState.router,
        ~basePath=routerState.basePath,
        ~id,
        ~params,
        ~searchParams,
        (),
      )
    ) {
    | Some(href) => navigateHref(~replace, href)
    | None => ()
    };
};

let useRouter = () => {
  let routerState = useRouterState();
  let navigateToRoute = useNavigateToRoute();
  let push = href => navigateHref(resolveHref(~basePath=routerState.basePath, href));
  let replace = href => navigateHref(~replace=true, resolveHref(~basePath=routerState.basePath, href));
  let pushRoute = (~id, ~params=Params.empty, ~searchParams=SearchParams.empty, ()) =>
    navigateToRoute(~id, ~params, ~searchParams, ());
  let replaceRoute = (~id, ~params=Params.empty, ~searchParams=SearchParams.empty, ()) =>
    navigateToRoute(~replace=true, ~id, ~params, ~searchParams, ());

  {push, replace, pushRoute, replaceRoute};
};

let useIsActiveHref = (~href, ~exact=false, ()) => {
  let routerState = useRouterState();
  let resolvedHref = resolveHref(~basePath=routerState.basePath, href);
  let targetPathname =
    resolvedHref
    |> pathnameOfHref
    |> (path =>
         switch (stripBasePath(~basePath=routerState.basePath, ~path)) {
         | Some(pathname) => pathname
         | None => path
         }
       );
  let currentPathname = routerState.pathname;

  if (exact || targetPathname == "/") {
    currentPathname == targetPathname;
  } else {
    currentPathname == targetPathname || String.starts_with(currentPathname, ~prefix=targetPathname ++ "/");
  };
};

module Link = {
  [@react.component]
  let make = (
    ~href,
    ~replace=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~children,
  ) => {
    let routerState = useRouterState();
    let hrefValue = resolveHref(~basePath=routerState.basePath, href);

    switch%platform (Runtime.platform) {
    | Client => {
        let router = useRouter();
        let handleClick = event => {
          event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
          replace ? router.replace(href) : router.push(href);
        };
        <a id=?elementId className=?className href=hrefValue onClick=handleClick>
          children
        </a>;
      }
    | Server =>
      <a id=?elementId className=?className href=hrefValue> children </a>
    };
  };
};

module NavLink = {
  [@react.component]
  let make = (
    ~href,
    ~replace=false,
    ~exact=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~activeClassName: option(string)=?,
    ~children,
  ) => {
    let isActive = useIsActiveHref(~href, ~exact, ());
    let mergedClassName =
      switch (className, activeClassName, isActive) {
      | (Some(baseClassName), Some(activeClassName), true) =>
        Some(joinClassNames([baseClassName, activeClassName]))
      | (Some(baseClassName), _, _) => Some(baseClassName)
      | (None, Some(activeClassName), true) => Some(activeClassName)
      | _ => None
      };

    <Link
      href
      replace
      elementId=?elementId
      className=?mergedClassName>
      children
    </Link>;
  };
};

module RouteLink = {
  [@react.component]
  let make = (
    ~id,
    ~params=Params.empty,
    ~searchParams=SearchParams.empty,
    ~replace=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~children,
  ) => {
    let hrefValue =
      useRouteHref(~id, ~params, ~searchParams, ()) |> Option.value(~default="#");

    switch%platform (Runtime.platform) {
    | Client => {
        let router = useRouter();
        let handleClick = event => {
          event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
          replace
            ? router.replaceRoute(~id, ~params, ~searchParams, ())
            : router.pushRoute(~id, ~params, ~searchParams, ());
        };
        <a id=?elementId className=?className href=hrefValue onClick=handleClick>
          children
        </a>;
      }
    | Server =>
      <a id=?elementId className=?className href=hrefValue> children </a>
    };
  };
};

module RouteNavLink = {
  [@react.component]
  let make = (
    ~id,
    ~params=Params.empty,
    ~searchParams=SearchParams.empty,
    ~replace=false,
    ~exact=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~activeClassName: option(string)=?,
    ~children,
  ) => {
    let isActive = useIsActiveRoute(~id, ~exact, ());
    let mergedClassName =
      switch (className, activeClassName, isActive) {
      | (Some(baseClassName), Some(activeClassName), true) =>
        Some(joinClassNames([baseClassName, activeClassName]))
      | (Some(baseClassName), _, _) => Some(baseClassName)
      | (None, Some(activeClassName), true) => Some(activeClassName)
      | _ => None
      };

    <RouteLink
      id
      params
      searchParams
      replace
      elementId=?elementId
      className=?mergedClassName>
      children
    </RouteLink>;
  };
};

[@react.component]
let make = (
   ~router: t('state),
   ~state: option('state)=?,
   ~basePath: option(string)=?,
   ~serverPathname: option(string)=?,
   ~serverSearch="",
) => {
  let hydrationState = readHydrationState();
  let effectiveBasePath =
    switch (basePath, hydrationState) {
    | (Some(value), _) => normalizeBasePath(value)
    | (None, Some(state)) => state.basePath
    | (None, None) => "/"
    };

  let serverUrl =
    switch (serverPathname, hydrationState) {
    | (Some(pathname), _) =>
      Some(makeServerUrl(~basePath=effectiveBasePath, ~pathname, ~search=serverSearch, ()))
    | (None, Some(state)) =>
      Some(
        makeServerUrl(
          ~basePath=state.basePath,
          ~pathname=state.pathname,
          ~search=state.search,
          (),
        )
      )
    | (None, None) => None
    };

  let initialUrl = ReasonReactRouter.useUrl(~serverUrl=?serverUrl, ());
  let (url, setUrl) = React.useState(() => initialUrl);

  React.useEffect0(() => {
    switch%platform (Runtime.platform) {
    | Client =>
      let watcherId =
        ReasonReactRouter.watchUrl(nextUrl => setUrl(_previous => nextUrl));
      Some(() => ReasonReactRouter.unwatchUrl(watcherId))
     | Server => None
     }
   });

  let currentBrowserPath = url.path |> pathOfSegments;
  let currentPathname =
    switch (stripBasePath(~basePath=effectiveBasePath, ~path=currentBrowserPath)) {
    | Some(pathname) => pathname
    | None => normalizePath(currentBrowserPath)
    };
  let search = url.search;
  let searchParams = SearchParams.parse(search);
  let matchResult =
    matchPath(
      ~router,
      ~pathname=currentPathname,
      ~searchParams,
      (),
    );

  let renderedElement =
    switch (matchResult) {
    | Some(result) => renderMatched(~matchResult=result, ~router)
    | None => renderNotFound(~router, ~pathname=currentPathname)
    };

  let title = resolveTitle(~router, ~matchResult, ~state);
  let headTags = resolveHeadTags(~router, ~matchResult, ~state);
  let headVersion =
    title ++ "\n" ++ (headTags |> List.map(serializeHeadTag) |> String.concat("|"));

  React.useEffect1(() => {
    switch%platform (Runtime.platform) {
    | Client => {
        setDocumentTitleValue(documentRef, title);
        applyHeadTags(headTags);
        None;
      }
    | Server => None
    }
  }, [|headVersion|]);

  let routerState =
    Some({
      router: castToUnitRouter(router),
      basePath: effectiveBasePath,
      pathname: currentPathname,
      search,
      searchParams,
      params:
        switch (matchResult) {
        | Some(result) => result.params
        | None => Params.empty
        },
      matchResult: castToUnitMatchResult(matchResult),
    });

  switch%platform (Runtime.platform) {
  | Client =>
    React.createElement(
      provider,
      {
        "value": routerState,
        "children": renderedElement,
      },
    )
  | Server =>
    React.Context.provider(
      context,
      {
        "value": routerState,
        "children": renderedElement,
      },
    )
  };
};
