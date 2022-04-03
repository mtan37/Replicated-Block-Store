while true
do
    echo "running ./server " $@
    ./server $@
    sleep 3
    echo ""
    echo ""
    echo "*****************"
    echo "Restarting server"
    echo "*****************"
    echo ""
done