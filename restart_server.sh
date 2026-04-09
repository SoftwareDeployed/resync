>&2 echo "--- 1) Checking if server is running ---"
pkill -f server.exe || echo "No existing server process"
sleep 1

>&2 echo "--- 2) Building server ---"
cd /root/resync
eval $(opam env --switch=/root/resync --set-switch)
dune build demos/video-chat/server/src/server.exe

>&2 echo "--- 3) Starting server ---"
nohup env SERVER_INTERFACE=0.0.0.0 VIDEO_CHAT_SERVER_PORT=80 VIDEO_CHAT_DOC_ROOT='./_build/default/demos/video-chat/ui/src/' VIDEO_CHAT_BASE_URL='https://video-chat-demo.softwaredeployed.com' _build/default/demos/video-chat/server/src/server.exe > /tmp/server.log 2>&1 &
sleep 2

>&2 echo "--- 4) Checking PID ---"
pgrep -f server.exe || echo "Server not running"