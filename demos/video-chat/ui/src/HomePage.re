[@react.component]
let make = () => {
  let (roomId, setRoomId) = React.useState(() => "");
  let router = UniversalRouter.useRouter();

  let createRoom = () => {
    let newRoomId = UUID.make();
    router.push("/room/" ++ newRoomId);
  };

  let joinRoom = () => {
    if (roomId != "") {
      router.push("/room/" ++ roomId);
    };
  };

  <div id="home-page" className="min-h-screen bg-gray-900 flex items-center justify-center p-4">
    <div className="max-w-md w-full bg-gray-800 rounded-xl shadow-2xl p-8">
      <h1 className="text-3xl font-bold text-white mb-2 text-center">
        {React.string("Video Chat")}
      </h1>
      <p className="text-gray-400 text-center mb-8">
        {React.string("1-on-1 video conferencing")}
      </p>
      
      <div className="space-y-4">
        <button
          id="create-room-button"
          onClick={_ => createRoom()}
          className="w-full bg-blue-600 hover:bg-blue-700 text-white font-semibold py-3 px-4 rounded-lg transition duration-200 flex items-center justify-center gap-2">
          <Lucide.IconVideo size=20 />
          {React.string("Create New Room")}
        </button>
        
        <div className="relative">
          <div className="absolute inset-0 flex items-center">
            <div className="w-full border-t border-gray-600" />
          </div>
          <div className="relative flex justify-center text-sm">
            <span className="px-2 bg-gray-800 text-gray-400">
              {React.string("or")}
            </span>
          </div>
        </div>
        
        <div className="flex gap-2">
          <input
            id="join-room-input"
            type_="text"
            value=roomId
            onChange={e => setRoomId(React.Event.Form.target(e)##value)}
            placeholder="Enter room ID"
            className="flex-1 bg-gray-700 text-white placeholder-gray-400 rounded-lg px-4 py-3 focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <button
            id="join-room-button"
            onClick={_ => joinRoom()}
            disabled={roomId == ""}
            className="bg-green-600 hover:bg-green-700 disabled:bg-gray-600 disabled:cursor-not-allowed text-white font-semibold py-3 px-4 rounded-lg transition duration-200">
            <Lucide.IconLogIn size=20 />
          </button>
        </div>
      </div>
      
      <div className="mt-8 text-center text-sm text-gray-500">
        {React.string("Share the room ID with someone to start a call")}
      </div>
    </div>
  </div>;
};
