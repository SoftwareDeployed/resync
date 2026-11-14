let tier = amount =>
  if (amount < 50) {
    ("small", "#5b8db8");
  } else if (amount < 200) {
    ("medium", "#4e9b6a");
  } else if (amount < 500) {
    ("large", "#d5b566");
  } else {
    ("xlarge", "#a83c3c");
  };

let chipCount = amount => min(6, max(2, amount / 50 + 1));

[@react.component]
let make = (~amount: int) => {
  let (tierName, color) = tier(amount);
  <span className="seat-bet" ariaLabel={"Bet " ++ string_of_int(amount) ++ " chips"}>
    <span className={"chip-stack chip-tier-" ++ tierName} ariaHidden={true}>
      {Array.init(chipCount(amount), index =>
         <span
           className="chip-icon"
           key={string_of_int(index)}
           style={ReactDOM.Style.make(~left=string_of_int(index * 7) ++ "px", ~color, ())}>
           <svg viewBox="0 0 4000 4000" ariaHidden={true}>
             <circle
               className="chip-body"
               transform="matrix(0.709526 0.40964 -0.409645 0.709518 2000.02 1999.95)"
               r="1831"
             />
             <path
               className="chip-cutouts"
               d="M1729 3475c181,33 365,32 542,-1l0 -328 -542 0 0 329zm833 -751l42 72c-67,51 -140,93 -217,125l-42 -73c77,-31 150,-73 217,-124zm-437 183l0 84c-82,11 -166,11 -250,1l0 -84c84,11 168,11 250,-1zm-729 -110l42 -73c32,25 67,49 104,70 36,21 74,39 112,55l-42 73c-38,-16 -75,-35 -112,-56 -37,-21 -71,-44 -104,-69zm-808 -295l284 -164 271 469 -285 165c-119,-140 -209,-300 -270,-470zm2261 -156l73 42c-16,38 -35,75 -56,112 -21,37 -44,71 -69,104l-73 -42c25,-32 49,-67 70,-104 21,-36 39,-74 55,-112zm-1646 -950l73 42c-25,32 -49,67 -70,104 -21,36 -39,74 -55,112l-73 -42c16,-38 35,-75 56,-112 21,-37 44,-71 69,-104zm410 -317l42 73c-77,31 -150,73 -217,124l-42 -72c67,-51 140,-93 217,-125zm512 -71l0 84c-84,-11 -168,-11 -250,1l0 -84c83,-11 166,-11 250,-1zm221 143l42 -73c38,16 75,35 112,56 37,21 71,44 104,69l-42 73c-32,-25 -67,-49 -104,-70 -36,-21 -74,-39 -112,-55zm561 724l84 0c11,83 11,166 1,250l-84 0c11,-83 11,-168 -1,-250zm-1899 0l84 0c-11,84 -11,168 1,250l-84 0c-11,-82 -11,-166 -1,-250zm1716 -437l72 -42c51,67 93,140 125,217l-73 42c-31,-77 -73,-150 -124,-217zm-1645 949l73 -42c31,77 73,150 124,217l-72 42c-51,-67 -93,-140 -125,-217zm2333 -889c-61,-170 -151,-330 -270,-470l-285 165 271 469 284 -164zm-1141 -973l0 329 -542 0 0 -328c178,-33 361,-34 542,-1zm-1413 503l285 165 -271 469 -285 -165c30,-84 68,-167 114,-247 46,-80 99,-154 157,-222zm2284 1944c58,-68 111,-142 157,-222 46,-80 84,-163 114,-247l-285 -165 -271 469 285 165z"
             />
           </svg>
         </span>
       )->React.array}
    </span>
    <span className="seat-bet-amount"> {React.string(string_of_int(amount))} </span>
  </span>;
};
