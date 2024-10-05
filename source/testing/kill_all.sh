SRC_DIR=/home/
HOSTFILE=$SRC_DIR/testing/lg_hostfile

if [ "$#" -ne 1 ]; then
    echo "arg1: 0=client-server, 1=mesh-p2p, 2=hypercube"
    exit 2
fi

case $1 in
0)
    WHICH="client-server"
    ;;
1)
    WHICH="mesh-p2p"
    ;;
2)
    WHICH="hypercube"
    ;;
esac

while read host; do
    echo $host
    ssh $host $SRC_DIR/testing/kill_client.sh $WHICH &
    sleep 0.1
done <$HOSTFILE
sleep 1
$host $SRC_DIR/testing/kill_server.sh
