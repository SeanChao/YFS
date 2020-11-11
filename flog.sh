#/bin/zsh
tmux new-window "tail -f extent_server.log"
tmux split-window -h "tail -f yfs_client1.log"
tmux split-window -v "tail -f yfs_client2.log"

