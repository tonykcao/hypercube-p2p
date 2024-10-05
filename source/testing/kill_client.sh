PROCESSES=$(ps aux | grep c_$1 | grep -v grep | awk '{print $2}')
for i in $PROCESSES; do
    kill $i
done
