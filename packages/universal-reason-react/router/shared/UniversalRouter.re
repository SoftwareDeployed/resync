exception MissingContext(string);

module Params = {
  type entry = (string, string);
  type t = list(entry);

  let empty: t = [];

  let ofList = entries => entries;

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
};

module Query = {
  type entry = (string, string);
  type t = list(entry);

  let empty: t = [];

  let ofList = entries => entries;

  let parse = search => {
    let rawSearch =
      String.starts_with(search, ~prefix="?")
        ? search->String.sub(1, String.length(search) - 1) : search;

    rawSearch == ""
      ? []
      : rawSearch
        |> String.split_on_char('&')
        |> List.filter_map(entry =>
             switch (entry |> String.split_on_char('=')) {
             | [] => None
             | [key] => Some((key, ""))
             | [key, value, ..._] => Some((key, value))
             }
           );
  };

  let find = (key, query) =>
    query
    |> List.find_map(((existingKey, value)) =>
         existingKey == key ? Some(value) : None
       );

  let toString = query =>
    query
    |> List.map(((key, value)) => value == "" ? key : key ++ "=" ++ value)
    |> String.concat("&");
};

type headTag =
  | MetaTag(string, string)
  | PropertyTag(string, string)
  | LinkTag(string, string);

type titleResolver = (~path: string, ~params: Params.t, ~query: Query.t) => string;

type headTagsResolver =
  (~path: string, ~params: Params.t, ~query: Query.t) => list(headTag);

type hydrationState = {
  routeRoot: string,
  path: string,
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
      ~query: Query.t,
      unit,
    ) =>
    React.element;
};

module type PAGE = {
  let make: (~params: Params.t, ~query: Query.t, unit) => React.element;
};

module type NOT_FOUND = {
  let make: (~path: string, unit) => React.element;
};

type routeConfig = {
  id: option(string),
  path: string,
  layout: option(module LAYOUT),
  page: option(module PAGE),
  title: option(string),
  resolveTitle: option(titleResolver),
  headTags: list(headTag),
  resolveHeadTags: option(headTagsResolver),
  children: list(routeConfig),
};

type t = {
  document: documentConfig,
  notFound: option(module NOT_FOUND),
  routes: list(routeConfig),
};

type matchResult = {
  path: string,
  params: Params.t,
  query: Query.t,
  routes: list(routeConfig),
};

type contextValue = {
  router: t,
  routeRoot: string,
  path: string,
  query: Query.t,
  params: Params.t,
  matchResult: option(matchResult),
};

let context: React.Context.t(option(contextValue)) = React.createContext(None);
let provider = React.Context.provider(context);

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

let normalizeRouteRoot = routeRoot => {
  let trimmed = routeRoot |> String.trim;
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
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  children,
  (),
) => {
  id,
  path: normalizeRoutePath(path),
  layout,
  page,
  title,
  resolveTitle,
  headTags,
  resolveHeadTags,
  children,
};

let group = (
  ~id: option(string)=?,
  ~path="",
  ~layout=?,
  ~title: option(string)=?,
  ~resolveTitle: option(titleResolver)=?,
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  children,
  (),
) =>
  route(
    ~id?,
    ~path,
    ~layout?,
    ~title?,
    ~resolveTitle?,
    ~headTags,
    ~resolveHeadTags?,
    children,
    (),
  );

let index = (
  ~id: option(string)=?,
  ~layout=?,
  ~page,
  ~title: option(string)=?,
  ~resolveTitle: option(titleResolver)=?,
  ~headTags=[],
  ~resolveHeadTags: option(headTagsResolver)=?,
  (),
) =>
  route(
    ~id?,
    ~path="",
    ~layout?,
    ~page,
    ~title?,
    ~resolveTitle?,
    ~headTags,
    ~resolveHeadTags?,
    [],
    (),
  );

let create = (~document: documentConfig=document(), ~notFound=?, routes) => {
  document,
  notFound,
  routes,
};

let serializeHydrationState = (state: hydrationState) =>
  state.routeRoot ++ "\n" ++ state.path ++ "\n" ++ state.search;

let parseHydrationState = value => {
  switch (value |> String.split_on_char('\n')) {
  | [routeRoot, path, search, ..._] =>
    Some({routeRoot: normalizeRouteRoot(routeRoot), path, search})
  | [routeRoot, path] =>
    Some({routeRoot: normalizeRouteRoot(routeRoot), path, search: ""})
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

let makeServerUrl = (~path, ~search="", ()) => {
  ReasonReactRouter.path: path |> segmentsOfPath,
  hash: "",
  search,
};

let stripRouteRoot = (~routeRoot, ~path) => {
  let normalizedRouteRoot = normalizeRouteRoot(routeRoot);
  let normalizedPath = normalizePath(path);

  if (normalizedRouteRoot == "/") {
    Some(normalizedPath);
  } else if (normalizedPath == normalizedRouteRoot) {
    Some("/");
  } else {
    let prefix = normalizedRouteRoot ++ "/";
    String.starts_with(normalizedPath, ~prefix)
      ? Some(
          normalizedPath->String.sub(
            String.length(normalizedRouteRoot),
            String.length(normalizedPath) - String.length(normalizedRouteRoot),
          ),
        )
      : None;
  };
};

let prefixRouteRoot = (~routeRoot, ~path) => {
  let normalizedRouteRoot = normalizeRouteRoot(routeRoot);
  let normalizedPath = normalizePath(path);

  if (normalizedRouteRoot == "/") {
    normalizedPath;
  } else if (normalizedPath == "/") {
    normalizedRouteRoot;
  } else {
    normalizedRouteRoot ++ normalizedPath;
  };
};

let candidateRouteRoots = path => {
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

let matchPath = (~router: t, ~path, ~query=Query.empty, ()) => {
  let pathSegments = path |> normalizePath |> segmentsOfPath;

  let rec matchRoutes =
          (
            routes: list(routeConfig),
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
          currentRoute: routeConfig,
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
            path: normalizePath(path),
            params: nextParams,
            query,
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

let matchMountedPath = (~router: t, ~routeRoot, ~path, ~query=Query.empty, ()) =>
  switch (stripRouteRoot(~routeRoot, ~path)) {
  | None => None
  | Some(rootlessPath) => matchPath(~router, ~path=rootlessPath, ~query, ())
  };

let rec findPatternForId = (routes: list(routeConfig), parentSegments, id) =>
  switch (routes) {
  | [] => None
  | [(currentRoute: routeConfig), ...remainingRoutes] =>
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

let buildPath = (~patternSegments, ~params=Params.empty, ~query=Query.empty, ()) => {
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
  let queryString = Query.toString(query);
  queryString == "" ? path : path ++ "?" ++ queryString;
};

let href = (~router: t, ~routeRoot="/", ~id, ~params=Params.empty, ~query=Query.empty, ()) =>
  switch (findPatternForId(router.routes, [], id)) {
  | None => None
  | Some(patternSegments) =>
    Some(
      prefixRouteRoot(
        ~routeRoot,
        ~path=buildPath(~patternSegments, ~params, ~query, ()),
      ),
    )
  };

let renderMatched = (~matchResult: matchResult, ~router: t) => {
  let _ = router;
  let params = matchResult.params;
  let query = matchResult.query;

  let rec loop = routes =>
    switch (routes) {
    | [] => React.null
    | [currentRoute] => {
        let leaf =
          switch (currentRoute.page) {
          | None => React.null
          | Some(page) =>
            module Page = (val page: PAGE);
            Page.make(~params, ~query, ())
          };

        switch (currentRoute.layout) {
        | None => leaf
        | Some(layout) =>
          module Layout = (val layout: LAYOUT);
          Layout.make(~children=leaf, ~params, ~query, ())
        }
      }
    | [currentRoute, ...remainingRoutes] => {
        let children = loop(remainingRoutes);
        switch (currentRoute.layout) {
        | None => children
        | Some(layout) =>
          module Layout = (val layout: LAYOUT);
          Layout.make(~children, ~params, ~query, ())
        }
      }
    };

  loop(matchResult.routes);
};

let renderNotFound = (~router: t, ~path) =>
  switch (router.notFound) {
  | None => React.null
  | Some(notFound) =>
    module NotFound = (val notFound: NOT_FOUND);
    NotFound.make(~path, ())
  };

let resolveTitle = (~router: t, ~matchResult: option(matchResult)) =>
  switch (matchResult) {
  | None => router.document.title
  | Some(result) =>
    result.routes
    |> List.fold_left(
         (resolvedTitle, currentRoute) =>
            switch (currentRoute.resolveTitle, currentRoute.title) {
            | (Some(resolveTitle), _) =>
              resolveTitle(~path=result.path, ~params=result.params, ~query=result.query)
            | (None, Some(title)) => title
            | (None, None) => resolvedTitle
            },
          router.document.title,
        )
  };

let resolveHeadTags = (~router: t, ~matchResult: option(matchResult)) =>
  switch (matchResult) {
  | None => router.document.headTags
  | Some(result) =>
    result.routes
    |> List.fold_left(
         (resolvedHeadTags, currentRoute) =>
           mergeHeadTags(
             resolvedHeadTags,
             switch (currentRoute.resolveHeadTags) {
             | Some(resolveHeadTags) =>
               mergeHeadTags(
                 currentRoute.headTags,
                 resolveHeadTags(
                   ~path=result.path,
                   ~params=result.params,
                   ~query=result.query,
                 ),
               )
             | None => currentRoute.headTags
             },
           ),
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
  ~router: t,
  ~children,
  ~routeRoot="/",
  ~path="/",
  ~search="",
  ~serializedState="",
  (),
) => {
  let query = Query.parse(search);
  let matchResult = matchMountedPath(~router, ~routeRoot, ~path, ~query, ());
  let title = resolveTitle(~router, ~matchResult);
  let head = mergeHeadContent(router.document.head, resolveHeadTags(~router, ~matchResult));
  let hydrationState = {routeRoot: normalizeRouteRoot(routeRoot), path, search};
  let afterMain =
    mergeAfterMain(router.document.afterMain, renderHydrationScript(hydrationState));

  <Document
    title
    lang=router.document.lang
    stylesheets=router.document.stylesheets
    scripts=router.document.scripts
    serializedState
    rootId=router.document.rootId
    head=?head
    beforeMain=?router.document.beforeMain
    afterMain=?afterMain>
    children
  </Document>;
};

let useRouter = () =>
  switch (React.useContext(context)) {
  | Some(value) => value
  | None => raise(MissingContext("UniversalRouter.useRouter requires the router provider"))
  };

let joinClassNames = classNames =>
  classNames
  |> List.filter(className => className != "")
  |> String.concat(" ");

let useParams = () => useRouter().params;
let useQuery = () => useRouter().query;
let useMatch = () => useRouter().matchResult;
let useCurrentPath = () => useRouter().path;
let useRouteRoot = () => useRouter().routeRoot;

let useIsActive = (~id, ~exact=false, ()) => {
  let routerState = useRouter();

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

let useHref = (~id, ~params=Params.empty, ~query=Query.empty, ()) => {
  let routerState = useRouter();
  href(
    ~router=routerState.router,
    ~routeRoot=routerState.routeRoot,
    ~id,
    ~params,
    ~query,
    (),
  );
};

let navigate = (~replace=false, path) =>
  replace ? ReasonReactRouter.replace(path) : ReasonReactRouter.push(path);

let useNavigateTo = () => {
  let routerState = useRouter();
  (~replace=false, ~id, ~params=Params.empty, ~query=Query.empty, ()) =>
    switch (
      href(
        ~router=routerState.router,
        ~routeRoot=routerState.routeRoot,
        ~id,
        ~params,
        ~query,
        (),
      )
    ) {
    | Some(path) => navigate(~replace, path)
    | None => ()
    };
};

module Link = {
  [@react.component]
  let make = (
    ~id,
    ~params=Params.empty,
    ~query=Query.empty,
    ~replace=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~children,
  ) => {
    let hrefValue = useHref(~id, ~params, ~query, ()) |> Option.value(~default="#");

    switch%platform (Runtime.platform) {
    | Client => {
        let navigateTo = useNavigateTo();
        let handleClick = event => {
          event->React.Event.toSyntheticEvent->React.Event.Synthetic.preventDefault;
          navigateTo(~replace, ~id, ~params, ~query, ());
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
    ~id,
    ~params=Params.empty,
    ~query=Query.empty,
    ~replace=false,
    ~exact=false,
    ~elementId: option(string)=?,
    ~className: option(string)=?,
    ~activeClassName: option(string)=?,
    ~children,
  ) => {
    let isActive = useIsActive(~id, ~exact, ());
    let mergedClassName =
      switch (className, activeClassName, isActive) {
      | (Some(baseClassName), Some(activeClassName), true) =>
        Some(joinClassNames([baseClassName, activeClassName]))
      | (Some(baseClassName), _, _) => Some(baseClassName)
      | (None, Some(activeClassName), true) => Some(activeClassName)
      | _ => None
      };

    <Link
      id
      params
      query
      replace
      elementId=?elementId
      className=?mergedClassName>
      children
    </Link>;
  };
};

[@react.component]
let make = (
  ~router: t,
  ~routeRoot: option(string)=?,
  ~serverPath: option(string)=?,
  ~serverSearch="",
) => {
  let hydrationState = readHydrationState();
  let effectiveRouteRoot =
    switch (routeRoot, hydrationState) {
    | (Some(value), _) => normalizeRouteRoot(value)
    | (None, Some(state)) => state.routeRoot
    | (None, None) => "/"
    };

  let serverUrl =
    switch (serverPath, hydrationState) {
    | (Some(path), _) => Some(makeServerUrl(~path, ~search=serverSearch, ()))
    | (None, Some(state)) =>
      Some(makeServerUrl(~path=state.path, ~search=state.search, ()))
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

  let currentPath = url.path |> pathOfSegments;
  let query = Query.parse(url.search);
  let matchResult =
    matchMountedPath(
      ~router,
      ~routeRoot=effectiveRouteRoot,
      ~path=currentPath,
      ~query,
      (),
    );

  let renderedElement =
    switch (matchResult) {
    | Some(result) => renderMatched(~matchResult=result, ~router)
    | None => renderNotFound(~router, ~path=currentPath)
    };

  let title = resolveTitle(~router, ~matchResult);
  let headTags = resolveHeadTags(~router, ~matchResult);
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
      router,
      routeRoot: effectiveRouteRoot,
      path: currentPath,
      query,
      params:
        switch (matchResult) {
        | Some(result) => result.params
        | None => Params.empty
        },
      matchResult,
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
  | Server => provider(~value=routerState, ~children=renderedElement, ())
  };
};
