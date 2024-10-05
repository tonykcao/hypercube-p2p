if [ "$#" -ne 2 ]; then
    echo "arg1: 0=client-server, 1=mesh-p2p, 3=hypercube"
    echo "arg2: 0=infinite, n=each client sends n messages"
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

SRC_DIR=/home/
TEST_DIR=$SRC_DIR/$WHICH
LOGS_DIR=$SRC_DIR/testing/logs_$WHICH/$(date +%Y%m%d_%H%M%S)
HOSTFILE=$SRC_DIR/testing/lg_hostfile
SERVER_IP=$(hostname --ip-address)
ITERS=$2

mkdir -p $LOGS_DIR

fuser -k 1888/tcp
sleep 1
$TEST_DIR/s_$WHICH >$LOGS_DIR/0server.txt &
sleep 2

ID=1
while read host; do
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))
    ssh $host $SRC_DIR/testing/test.sh $WHICH $SERVER_IP $ID $ITERS 1>$LOGS_DIR/$ID$host$ID.txt 2>>$LOGS_DIR/$host$ID.txt &
    ID=$((ID+1))

done <$HOSTFILE
